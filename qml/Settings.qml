import QtQuick 2.6
import Sailfish.Silica 1.0
import Aurora.Controls 1.0
Page {
 id: accountPage
 objectName: "accountPage"
 property bool pageReady: false
 property bool applyingSavedPassword: false
 property bool savedAccountMatchesForm: true
 ConnectionStatus {
  id: connectionStatusBar
  objectName: "connectionStatusBar"
  anchors.left: parent.left
  anchors.right: parent.right
  anchors.bottom: parent.bottom
 }
 Component.onDestruction: phone.setAccountName(accountNameField.text)
 Component.onCompleted: { pageReady=true; if (phone.hasSavedPassword()) phone.loadPassword() }
 AppBar { id: appBar; headerText: "Аккаунт"; AppBarSpacer {} }
 SilicaFlickable { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.topMargin: appBar.height + SafeZoneRect.insets.top; anchors.bottom: connectionStatusBar.top; clip: true; contentHeight: column.height
  Column { id: column; width: parent.width
   TextField { id: accountNameField; width: parent.width; label: "Название аккаунта"; text: phone.accountName; onActiveFocusChanged: { if (!activeFocus) phone.setAccountName(text) } }
   Label { x: Theme.horizontalPageMargin; width: parent.width-2*x; text: phone.storageError; visible: text.length>0; textFormat: Text.PlainText; wrapMode: Text.Wrap }
   SectionHeader { text: "Подключение · TLS" }
   TextField { id: host; property bool edited: false; onTextChanged: { if(pageReady && !applyingSavedPassword)edited=true }
    width: parent.width; label: "Сервер"; text: phone.setting("host",""); inputMethodHints: Qt.ImhNoAutoUppercase }
   TextField { id: domain; property bool edited: false; onTextChanged: { if(pageReady && !applyingSavedPassword)edited=true }
    width: parent.width; label: "Домен"; text: phone.setting("domain",""); inputMethodHints: Qt.ImhNoAutoUppercase }
   TextField { id: port; property bool edited: false; onTextChanged: { if(pageReady && !applyingSavedPassword)edited=true }
    width: parent.width; label: "Порт TLS"; text: phone.setting("port",5061); inputMethodHints: Qt.ImhDigitsOnly }
   TextField { id: user; property bool edited: false; onTextChanged: { if(pageReady && !applyingSavedPassword)edited=true }
    width: parent.width; label: "Имя пользователя"; text: phone.setting("user",""); inputMethodHints: Qt.ImhNoAutoUppercase }
   PasswordField { id: password; objectName: "accountPassword"; property bool edited: false; width: parent.width; label: "Пароль"; onTextChanged: { if (pageReady && !applyingSavedPassword) edited = true } }
   Connections {
    target: phone
    onAccountLoaded: {
     applyingSavedPassword=true
     if(!host.edited)host.text=profile.host
     if(!domain.edited)domain.text=profile.domain
     if(!port.edited)port.text=String(profile.port)
     if(!user.edited)user.text=profile.user
     if(!srtp.edited)srtp.checked=profile.srtp
     applyingSavedPassword=false
     savedAccountMatchesForm=host.text.trim()===profile.host && domain.text.trim()===profile.domain && Number(port.text)===profile.port && user.text.trim()===profile.user
    }
    onPasswordLoaded: { if (!password.edited && savedAccountMatchesForm) { applyingSavedPassword=true; password.text=secret; applyingSavedPassword=false } }
   }
   TextSwitch { id: srtp; property bool edited: false; onCheckedChanged: { if(pageReady && !applyingSavedPassword)edited=true }
    onClicked: phone.setRequireSecureAudio(checked)
    text: "Требовать шифрование аудио (SRTP)"; checked: phone.setting("srtp",false) }
   Item {
    width: parent.width; height: savePasswordButton.height + 2 * Theme.paddingMedium
    Button { id: savePasswordButton; anchors.centerIn: parent; text: "Сохранить"; enabled: password.text.length>0 && !phone.credentialsBusy; onClicked: { if(phone.saveAccount(host.text,domain.text,Number(port.text),user.text,srtp.checked))phone.savePassword(password.text) } }
   }
   Label { x: Theme.horizontalPageMargin; width: parent.width-2*x; text: phone.credentialStatus; wrapMode: Text.Wrap }
   Item {
    width: parent.width; height: connectionButton.height + 2 * Theme.paddingMedium
    Button { id: connectionButton; anchors.centerIn: parent; text: phone.disconnecting ? "Отключение…" : phone.connecting ? "Подключение…" : phone.registered ? "Отключиться" : "Подключиться"; enabled: !phone.active && !phone.connecting && !phone.disconnecting && (phone.registered || (!phone.credentialsBusy && password.text.length>0)); onClicked: { if (phone.registered) { phone.logout(); return } phone.login(host.text.trim(),domain.text.trim(),Number(port.text),user.text.trim(),password.text,srtp.checked); } }
   }

  }
 }
}
