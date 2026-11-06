import QtQuick 2.6
import QtQml.Models 2.1
import Sailfish.Silica 1.0
import Aurora.Controls 1.0

ApplicationWindow {
    id: applicationWindow
    property int currentTab: 0
    readonly property color connectionStatusColor: phone.connecting || phone.disconnecting ? "#e8b44c" : phone.registered ? "#42d9ae" : "#ef6b73"
    readonly property bool mainPageVisible: pageStack.currentPage && pageStack.currentPage.objectName === "dialerPage"
    readonly property var tabTitles: ["Набор", "История", "Контакты", "Избранное"]
    signal closeMenus()
    signal menuDismissed()
    onCurrentTabChanged: { closeMenus(); Qt.inputMethod.hide() }
    function closeAccount(){if(pageStack.depth>1)pageStack.pop()}
    function openMenuPage(file, name, properties) {
        closeMenus()
        Qt.inputMethod.hide()
        if(pageStack.currentPage && pageStack.currentPage.objectName===name)return
        var url=Qt.resolvedUrl(file)
        if(pageStack.depth>1)pageStack.replace(url,properties || {})
        else pageStack.push(url,properties || {})
    }
    function openContactsSettings(){openMenuPage("ContactsSettings.qml","contactsSettingsPage")}
    function openAccount(){openMenuPage("Settings.qml","accountPage")}
    function openAbout(){openMenuPage("About.qml","aboutPage")}
    initialPage: Component {
        Page {
            id: homePage
            objectName: "dialerPage"
            backNavigation: false
            property string contactsQuery: ""
            property string favoritesQuery: ""
            function selectTab(index) {
                applicationWindow.closeMenus()
                sectionPager.cancelFlick()
                sectionPager.currentIndex = index
                sectionPager.positionViewAtIndex(index, ListView.Beginning)
                applicationWindow.currentTab = index
            }
            function chooseNumber(value){dialer.setNumber(value);selectTab(0)}
            onStatusChanged: { if (status === PageStatus.Active) restorePager.restart() }
            Connections {
                target: applicationWindow
                onMenuDismissed: restorePager.restart()
            }
            Timer {
                id: restorePager
                interval: 0
                onTriggered: {
                    if(homePage.status!==PageStatus.Active)return
                    sectionPager.cancelFlick()
                    sectionPager.positionViewAtIndex(applicationWindow.currentTab, ListView.Beginning)
                }
            }
            NavigationBar {
                id: mainAppBar
                searchVisible: applicationWindow.currentTab===2 || applicationWindow.currentTab===3
                headerText: searchVisible ? "" : applicationWindow.currentTab===0 ? phone.accountName : applicationWindow.tabTitles[applicationWindow.currentTab]
                subHeaderText: {
                    if(applicationWindow.currentTab!==0)return ""
                    var status=phone.connecting ? "Подключение…" : phone.disconnecting ? "Отключение…" : phone.registered ? "Подключён" : "Не подключён"
                    return '<font color="'+applicationWindow.connectionStatusColor+'">'+status+'</font>'
                }
                searchText: applicationWindow.currentTab===2 ? homePage.contactsQuery : applicationWindow.currentTab===3 ? homePage.favoritesQuery : ""
                onSearchEdited: {
                    if(applicationWindow.currentTab===2)homePage.contactsQuery=text
                    else if(applicationWindow.currentTab===3)homePage.favoritesQuery=text
                }

            }
            // Keep page content beneath the overlay managed by PopupMenu.
            // Window-level siblings can otherwise cover that overlay.
            BottomNavigation {
                id: bottomNavigation
                objectName: "bottomNavigation"
                anchors.left: parent.left
                anchors.right: parent.right
                y: applicationWindow.mapToItem(parent, 0, applicationWindow.height - height).y
                z: 1
                currentIndex: applicationWindow.currentTab
                onSelected: homePage.selectTab(index)
            }
            SilicaListView {
                id: sectionPager
                objectName: "sectionPager"
                anchors.fill: parent
                anchors.topMargin: mainAppBar.height + SafeZoneRect.insets.top
                anchors.bottomMargin: Qt.inputMethod.visible ? 0 : bottomNavigation.height
                orientation: ListView.Horizontal
                flickableDirection: Flickable.HorizontalFlick
                snapMode: ListView.SnapOneItem
                boundsBehavior: Flickable.StopAtBounds
                highlightRangeMode: ListView.StrictlyEnforceRange
                preferredHighlightBegin: 0
                preferredHighlightEnd: 0
                highlightMoveDuration: 220
                clip: true
                currentIndex: 0
                onCurrentIndexChanged: { if(applicationWindow.currentTab!==currentIndex)applicationWindow.currentTab=currentIndex }
                model: ObjectModel {
                    DialerPage { id: dialer; width: sectionPager.width; height: sectionPager.height }
                    HistoryPage { width: sectionPager.width; height: sectionPager.height; onNumberSelected: homePage.chooseNumber(number) }
                    ContactsPage { searchText: homePage.contactsQuery; width: sectionPager.width; height: sectionPager.height; onNumberSelected: homePage.chooseNumber(number) }
                    FavoritesPage { searchText: homePage.favoritesQuery; width: sectionPager.width; height: sectionPager.height; onNumberSelected: homePage.chooseNumber(number) }
                }
            }
        }
    }
    cover: Component {
        CoverBackground {
            Column {
                anchors.centerIn: parent
                width: parent.width-2*Theme.paddingMedium
                spacing: Theme.paddingMedium
                Label { width: parent.width; text: appTitle; horizontalAlignment: Text.AlignHCenter; color: Theme.highlightColor }
                Label { width: parent.width; text: phone.registered ? "Подключён" : phone.connecting ? "Подключение…" : "Не подключён"; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; font.pixelSize: Theme.fontSizeSmall }
            }
        }
    }
}
