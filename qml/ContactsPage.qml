import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0
Item {
    id: contactsPage
    property string searchText: ""
    signal numberSelected(string number)
    ContactList {
        searchText: contactsPage.searchText
        anchors.fill: parent
        onNumberSelected: parent.numberSelected(number)
    }
}
