import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0

Page {
    objectName: "aboutPage"
    AppBar { id: appBar; headerText: "О приложении"; AppBarSpacer {} }
    SilicaFlickable {
        anchors.fill: parent
        anchors.topMargin: appBar.height + SafeZoneRect.insets.top
        contentHeight: info.height + 2 * Theme.paddingLarge
        Column {
            id: info
            x: Theme.horizontalPageMargin
            y: Theme.paddingLarge
            width: parent.width - 2 * x
            spacing: Theme.paddingLarge
            Label { text: appTitle; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: Theme.fontSizeExtraLarge; color: Theme.highlightColor }
            Label { text: "Версия " + appVersion; width: parent.width; wrapMode: Text.Wrap }
            Label { text: "IP-телефония для ОС Аврора с поддержкой FreePBX."; width: parent.width; wrapMode: Text.Wrap }
            Label { text: "Автор: " + appAuthorName; textFormat: Text.PlainText; width: parent.width; wrapMode: Text.Wrap }
            Label {
                text: appAuthorEmail
                textFormat: Text.PlainText
                width: parent.width
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                MouseArea { anchors.fill: parent; onClicked: Qt.openUrlExternally("mailto:" + appAuthorEmail) }
            }
            Label {
                text: "github.com/NightEugene/aurora-sip"
                textFormat: Text.PlainText
                width: parent.width
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                MouseArea { anchors.fill: parent; onClicked: Qt.openUrlExternally("https://github.com/NightEugene/aurora-sip") }
            }
            Label { text: "Лицензия GPL-2.0-or-later"; width: parent.width; wrapMode: Text.Wrap; color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeSmall }
        }
        VerticalScrollDecorator {}
    }
}
