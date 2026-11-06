import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0
Item {
    id: historyPage
    signal numberSelected(string number)
    HistoryList { anchors.fill: parent; onNumberSelected: parent.numberSelected(number) }
}
