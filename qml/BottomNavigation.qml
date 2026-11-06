import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0
Item {
    property int currentIndex: 0
    signal selected(int index)
    height: Theme.itemSizeLarge + Theme.paddingMedium + SafeZoneRect.insets.bottom
    Rectangle { width: parent.width; height: 1; color: Theme.rgba(Theme.primaryColor, 0.2) }
    Rectangle { x: currentIndex*parent.width/4; width: parent.width/4; height: 2; color: Theme.highlightColor }
    Row {
        width: parent.width
        Repeater {
            model: [{title:"Набор",icon:"dialpad"},{title:"История",icon:"history"},{title:"Контакты",icon:"contact"},{title:"Избранное",icon:"favorite"}]
            BackgroundItem {
                width: parent.width/4
                height: parent.parent.height - SafeZoneRect.insets.bottom
                onClicked: selected(index)
                Column {
                    anchors.centerIn: parent
                    spacing: Theme.paddingSmall
                    Icon { anchors.horizontalCenter: parent.horizontalCenter; source: "image://theme/icon-m-"+modelData.icon; color: currentIndex===index?Theme.highlightColor:Theme.primaryColor }
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.title; font.pixelSize: Theme.fontSizeExtraSmall; color: currentIndex===index?Theme.highlightColor:Theme.primaryColor }
                }
            }
        }
    }
}
