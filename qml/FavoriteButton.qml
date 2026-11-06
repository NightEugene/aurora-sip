import QtQuick 2.6
import Sailfish.Silica 1.0
IconButton {
    property string number
    property string contactName
    readonly property bool favoriteMarked: addressBook.favoritesRevision >= 0 && addressBook.isFavorite(number)
    readonly property url favoriteIcon: favoriteMarked ? "image://theme/icon-m-favorite-selected" : "image://theme/icon-m-favorite"
    objectName: "favorite-" + number
    icon.source: favoriteIcon
    icon.color: favoriteMarked ? Theme.highlightColor : Theme.secondaryColor
    onClicked: addressBook.toggleFavorite(contactName,number)
}
