import QtQuick 2.6
import Sailfish.Silica 1.0
import "HistoryFormat.js" as HistoryFormat
SilicaListView {
    signal numberSelected(string number)
    header: Label { width: parent.width; height: text.length ? implicitHeight+2*Theme.paddingMedium : 0; text: phone.storageError; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.highlightColor }
    model: phone.history
    delegate: BackgroundItem {
        property string contactName: {
            var revision=addressBook.namesRevision
            return addressBook.nameForNumber(modelData.peer)
        }
        height: Math.max(Theme.itemSizeLarge, historyColumn.height+2*Theme.paddingMedium)
        onClicked: { var number=addressBook.dialNumber(modelData.peer); if(number.length) numberSelected(number) }
        Column {
            id: historyColumn
            x: Theme.horizontalPageMargin
            width: parent.width-2*x
            anchors.verticalCenter: parent.verticalCenter
            Label { width: parent.width; text: contactName || modelData.peer; textFormat: Text.PlainText; truncationMode: TruncationMode.Fade }
            Label {
                visible: contactName.length>0
                width: parent.width
                text: addressBook.dialNumber(modelData.peer) || modelData.peer
                textFormat: Text.PlainText
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
            }
            Label { width: parent.width; textFormat: Text.PlainText; truncationMode: TruncationMode.Fade; text: Qt.formatDateTime(new Date(modelData.time), "dd.MM.yyyy hh:mm")+" · "+HistoryFormat.details(modelData); font.pixelSize: Theme.fontSizeExtraSmall; color: Theme.secondaryColor }
        }
    }
    ViewPlaceholder { enabled: count===0; text: "Звонков пока нет" }
    VerticalScrollDecorator {}
}
