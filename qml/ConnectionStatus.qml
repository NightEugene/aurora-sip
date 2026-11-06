import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0

Item {
    height: statusLabel.implicitHeight + 2 * Theme.paddingMedium + SafeZoneRect.insets.bottom
    Rectangle { anchors.fill: parent; color: Theme.rgba(Theme.primaryColor, 0.04) }
    Rectangle {
        x: Theme.horizontalPageMargin
        y: Theme.paddingMedium + Theme.fontSizeSmall / 2
        width: Theme.paddingSmall
        height: width
        radius: width / 2
        color: phone.registered ? "#42d9ae" : Theme.secondaryColor
    }
    Label {
        id: statusLabel
        x: Theme.horizontalPageMargin + Theme.paddingSmall + Theme.paddingMedium
        y: Theme.paddingMedium
        width: parent.width - x - Theme.horizontalPageMargin
        text: phone.accountName + " · " + phone.status
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.secondaryColor
    }
}
