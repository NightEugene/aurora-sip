import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0
Item {
    clip: true
    property bool favoritesOnly: false
    property string searchText: ""
    signal numberSelected(string number)
    function filteredEntries() {
        var rows=favoritesOnly?addressBook.favorites:addressBook.contacts
        var query=searchText.toLowerCase()
        var result=[]
        for(var i=0;i<rows.length;i++) if(!query || rows[i].name.toLowerCase().indexOf(query)>=0 || rows[i].number.indexOf(query)>=0) result.push(rows[i])
        return result
    }
    Label {
        id: cacheInfo
        visible: !favoritesOnly && addressBook.directory.usingSavedCopy && !addressBook.loading
        anchors.top: parent.top
        x: Theme.horizontalPageMargin
        width: parent.width-2*x
        height: visible ? implicitHeight+Theme.paddingSmall : 0
        text: addressBook.directory.cacheStatus
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
    }
    SilicaListView {
        id: contactsView
        property bool pullRefreshActive: false
        function refreshContacts() {
            pullRefreshActive=true
            if(!addressBook.loading)addressBook.refresh()
            if(!addressBook.loading && pullRefreshActive) {
                pullRefreshActive=false
                contactsView.PullToRefresh.refreshCompleted(false)
            }
        }
        PullToRefresh.enabled: !favoritesOnly && addressBook.directory.source.length>0
        PullToRefresh.refreshHandler: contactsView.refreshContacts
        clip: true
        anchors.top: cacheInfo.bottom
        anchors.bottom: storageInfo.top
        width: parent.width
        model: filteredEntries()
        delegate: ListItem {
            id: contactItem
            readonly property bool favoriteMarked: addressBook.favoritesRevision >= 0 && addressBook.isFavorite(modelData.number)
            width: contactsView.width
            contentHeight: Theme.itemSizeLarge
            menu: favoriteMenu
            Component {
                id: favoriteMenu
                ContextMenu {
                    MenuItem {
                        text: contactItem.favoriteMarked ? "Удалить из избранного" : "Добавить в избранное"
                        onClicked: addressBook.toggleFavorite(modelData.name, modelData.number)
                    }
                }
            }
            onClicked: numberSelected(modelData.number)
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width-2*x-(star.visible ? star.width+Theme.paddingMedium : 0)
                anchors.verticalCenter: parent.verticalCenter
                Label { width: parent.width; text: modelData.name; textFormat: Text.PlainText; truncationMode: TruncationMode.Fade }
                Label { width: parent.width; text: modelData.number; textFormat: Text.PlainText; color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeSmall }
            }
            Icon {
                id: star
                visible: contactItem.favoriteMarked
                source: "image://theme/icon-m-favorite-selected"
                color: Theme.highlightColor
                anchors.right: parent.right
                anchors.rightMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        ViewPlaceholder { enabled: contactsView.count===0 && !addressBook.loading; text: favoritesOnly?"В избранном пока нет номеров":addressBook.status.length?addressBook.status:"Контактов пока нет" }
        VerticalScrollDecorator {}
    }
    Connections {
        target: addressBook.directory
        onRefreshFinished: {
            if(contactsView.pullRefreshActive) {
                contactsView.pullRefreshActive=false
                contactsView.PullToRefresh.refreshCompleted(success)
            }
        }
    }
    Column {
        id: storageInfo
        anchors.bottom: parent.bottom
        width: parent.width
        visible: addressBook.storageError.length>0
        height: visible ? storageMessage.implicitHeight+retryButton.height+Theme.paddingMedium : 0
        Label { id: storageMessage; width: parent.width; text: addressBook.storageError; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.highlightColor }
        Button { id: retryButton; anchors.horizontalCenter: parent.horizontalCenter; text: "Повторить сохранение"; onClicked: addressBook.retrySave() }
    }
    BusyIndicator { anchors.centerIn: parent; running: addressBook.loading && !favoritesOnly && !contactsView.pullRefreshActive }
}
