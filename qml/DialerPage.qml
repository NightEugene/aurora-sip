import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0

Item {
    id: dialerPage
    readonly property string matchedContactName: {
        var revision=addressBook.namesRevision
        return addressBook.nameForNumber(phone.active ? phone.peer : dialNumber.text)
    }
    KeypadFeedback { id: keyFeedback }
    function setNumber(value) {dialNumber.text=value;dialNumber.cursorPosition=value.length;dialNumber.focus=false;Qt.inputMethod.hide()}
    function pressDigit(value) {
        if(phone.active)phone.dtmf(value)
        else {if(dialNumber.selectedText.length)dialNumber.remove(dialNumber.selectionStart,dialNumber.selectionEnd);dialNumber.insert(dialNumber.cursorPosition,value);dialNumber.focus=false;Qt.inputMethod.hide()}
    }
            SilicaFlickable {
                id: dialerViewport
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 0
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 0
                clip: true
                flickableDirection: Flickable.VerticalFlick
                contentHeight: Math.max(height, dialerBody.height + 2 * Theme.paddingLarge)
                Column {
                    id: dialerBody
                    width: parent.width
                    y: Math.max(Theme.paddingLarge, dialerViewport.height - height - Theme.paddingLarge)
                    spacing: Theme.paddingLarge
                    Label {
                        objectName: "dialContactName"
                        visible: dialerPage.matchedContactName.length>0
                        x: Theme.horizontalPageMargin
                        width: parent.width-2*x
                        text: dialerPage.matchedContactName
                        textFormat: Text.PlainText
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: Theme.primaryColor
                    }
                    Label { visible: phone.active; x: Theme.horizontalPageMargin; width: parent.width-2*x; text: phone.peer; textFormat: Text.PlainText; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                    Item {
                        width: parent.width
                        height: Theme.itemSizeMedium
                        TextInput {
                            id: dialNumber
                            objectName: "dialNumber"
                            anchors.centerIn: parent
                            width: parent.width - 2 * (erase.width + Theme.horizontalPageMargin)
                            height: parent.height
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeHuge
                            horizontalAlignment: TextInput.AlignHCenter
                            verticalAlignment: TextInput.AlignVCenter
                            inputMethodHints: Qt.ImhDialableCharactersOnly
                            readOnly: phone.active
                            selectByMouse: true
                            clip: true
                            maximumLength: 64
                        }
                        FavoriteButton {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.horizontalPageMargin
                            anchors.verticalCenter: parent.verticalCenter
                            visible: dialNumber.text.length > 0 && !phone.active
                            number: dialNumber.text
                            contactName: dialerPage.matchedContactName || dialNumber.text
                        }
                        IconButton {
                            id: erase
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.horizontalPageMargin
                            anchors.verticalCenter: parent.verticalCenter
                            visible: dialNumber.text.length > 0 && !phone.active
                            width: Theme.itemSizeLarge
                            icon.width: Theme.iconSizeMedium
                            icon.height: Theme.iconSizeMedium
                            icon.source: "image://theme/icon-l-backspace-keypad"
                            onPressed: keyFeedback.pulse()
                            onClicked: {
                                if (dialNumber.selectedText.length) dialNumber.remove(dialNumber.selectionStart, dialNumber.selectionEnd)
                                else if (dialNumber.cursorPosition > 0) dialNumber.remove(dialNumber.cursorPosition-1, dialNumber.cursorPosition)
                            }
                            onPressAndHold: dialNumber.text = ""
                        }
                    }
                    Grid {
                        id: keypad
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width * 0.74
                        columns: 3
                        Repeater {
                            model: ["1","2","3","4","5","6","7","8","9","*","0","#"]
                            BackgroundItem {
                                width: keypad.width/3
                                height: keypad.width/4
                                onPressed: keyFeedback.play(modelData)
                                onClicked: dialerPage.pressDigit(modelData)
                                onPressAndHold: { if (!phone.active && modelData === "0") dialerPage.pressDigit("+") }
                                Label { anchors.horizontalCenter: parent.horizontalCenter; y: (parent.height-height)/2-Theme.paddingSmall; text: modelData; font.pixelSize: Theme.fontSizeHuge }
                                Label { anchors.horizontalCenter: parent.horizontalCenter; y: parent.height*0.7; text: modelData === "0" ? "+" : ""; font.pixelSize: Theme.fontSizeSmall; color: Theme.secondaryColor }
                            }
                        }
                    }
                    Item {
                        width: parent.width
                        height: callButton.height + 2 * Theme.paddingLarge
                        visible: !phone.active || phone.incoming
                        BackgroundItem {
                            id: callButton
                            objectName: "callButton"
                            anchors.centerIn: parent
                            width: parent.width*0.36
                            height: width/2
                            visible: !phone.active || phone.incoming
                            enabled: phone.incoming || (phone.registered && dialNumber.text.length>0)
                            onPressed: keyFeedback.pulse()
                            onClicked: { dialNumber.focus=false; Qt.inputMethod.hide(); if(phone.incoming) phone.answer(); else phone.dial(dialNumber.text) }
                            Rectangle { anchors.fill: parent; radius: height/2; color: callButton.enabled ? (callButton.down ? "#396b3e" : "#48814d") : Theme.rgba(Theme.primaryColor,0.08); border.width: callButton.enabled ? 0 : 1; border.color: Theme.rgba(Theme.secondaryColor,0.3) }
                            Icon { anchors.centerIn: parent; source: "image://theme/icon-m-call"; color: callButton.enabled ? "white" : Theme.secondaryColor }
                            Accessible.name: phone.incoming ? "Принять звонок" : "Позвонить"
                        }
                    }
                    Button { anchors.horizontalCenter: parent.horizontalCenter; text: "Громкая связь"; visible: phone.active; onClicked: phone.toggleSpeaker() }
                    Button { anchors.horizontalCenter: parent.horizontalCenter; text: phone.muted ? "Включить микрофон" : "Выключить микрофон"; visible: phone.active; onClicked: phone.toggleMute() }
                    Button { anchors.horizontalCenter: parent.horizontalCenter; text: phone.incoming ? "Отклонить" : "Завершить"; visible: phone.active; onClicked: phone.hangup() }
                }
                VerticalScrollDecorator {}
            }

}
