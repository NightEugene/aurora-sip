import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0
import Sailfish.Pickers 1.0
Page {
 id: contactsSettingsPage
 objectName: "contactsSettingsPage"
 function chooseContactsFile() { Qt.inputMethod.hide(); pageStack.push(contactsFilePicker) }
 AppBar { id: appBar; headerText: "Контакты"; AppBarSpacer {} }
 SilicaFlickable {
  anchors.fill: parent
  anchors.topMargin: appBar.height + SafeZoneRect.insets.top
  contentHeight: column.height
  Column { id: column; width: parent.width
   TextField {
    id: contactsSource
    objectName: "contactsSource"
    width: parent.width
    label: "Файл контактов"
    placeholderText: "Ссылка или путь к XML-файлу"
    text: addressBook.directory.source
    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
   }
   Item {
    width: parent.width; height: chooseContactsButton.height + 2 * Theme.paddingMedium
    Button { id: chooseContactsButton; objectName: "chooseContactsFile"; anchors.centerIn: parent; text: "Выбрать файл"; enabled: !addressBook.loading; onClicked: contactsSettingsPage.chooseContactsFile() }
   }
   Item {
    width: parent.width; height: saveContactsButton.height + 2 * Theme.paddingMedium
    Button { id: saveContactsButton; anchors.centerIn: parent; text: addressBook.loading ? "Отменить обновление" : "Сохранить и обновить"; onClicked: { Qt.inputMethod.hide(); if(addressBook.loading)addressBook.directory.cancel(); else addressBook.directory.configure(contactsSource.text) } }
   }
   Label { x: Theme.horizontalPageMargin; width: parent.width-2*x; text: addressBook.status; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: Theme.fontSizeSmall }

  }
  VerticalScrollDecorator {}
 }
 Component {
  id: contactsFilePicker
  FilePickerPage {
   objectName: "contactsFilePicker"
   title: "Файл контактов"
   nameFilters: [ "*.xml" ]
   onSelectedContentPropertiesChanged: {
    if (selectedContentProperties && selectedContentProperties.filePath) contactsSource.text=selectedContentProperties.filePath
   }
  }
 }
}
