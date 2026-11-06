import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0
Item {
    id: favoritesPage
    property string searchText: ""
    signal numberSelected(string number)
    ContactList { searchText: favoritesPage.searchText; favoritesOnly: true; anchors.fill: parent; onNumberSelected: parent.numberSelected(number) }
}
