import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0

AppBar {
    id: bar
    objectName: "navigationBar"
    property bool menuVisible: true
    property bool searchVisible: false
    property string searchText: ""
    signal searchEdited(string text)
    function showMenu() { if(enabled && navigationMenu)navigationMenu.open() }
    function hideMenu() { if(navigationMenu)navigationMenu.close() }
    onEnabledChanged: if(!enabled)hideMenu()
    property Connections menuConnections: Connections {
        target: appNavigation
        onCloseMenus: bar.hideMenu()
    }
    AppBarSearchField {
        id: toolbarSearch
        objectName: "toolbarSearch"
        visible: bar.searchVisible
        enabled: visible
        placeholderText: "Поиск"
        backgroundEnabled: true
        onTextChanged: if(bar.searchVisible && text!==bar.searchText)bar.searchEdited(text)
    }
    property Binding searchBinding: Binding { target: toolbarSearch; property: "text"; value: bar.searchText }
    AppBarSpacer { visible: !bar.searchVisible }
    AppBarButton {
        visible: bar.menuVisible
        icon.source: "image://theme/icon-splus-more"
        onClicked: bar.showMenu()
        PopupMenu {
            id: navigationMenu
            objectName: "navigationMenu"
            onClosed: if(appNavigation)appNavigation.menuDismissed()
            PopupMenuItem {
                text: "Аккаунт"
                enabled: !phone.active
                onClicked: if(appNavigation)appNavigation.openAccount()
            }
            PopupMenuItem { text: "Контакты"; onClicked: if(appNavigation)appNavigation.openContactsSettings() }
            PopupMenuItem { text: "О приложении"; onClicked: if(appNavigation)appNavigation.openAbout() }
        }
    }
}
