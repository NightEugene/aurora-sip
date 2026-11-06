import QtQuick 2.6
import Nemo.Ngf 1.0

Item {
    id: feedback
    objectName: "keypadFeedback"
    visible: false
    readonly property bool toneConnected: keypadTone.connected
    readonly property bool hapticConnected: haptic.connected
    readonly property int tonesPlayed: keypadTone.played
    property int hapticsPlayed: 0
    property int hapticFailures: 0
    readonly property int failures: keypadTone.failures+hapticFailures
    function pulse() { haptic.stop(); haptic.play() }
    function play(digit) {
        if(!/^[0-9*#]$/.test(digit))return
        keypadTone.play(digit)
        pulse()
    }
    function stop() { keypadTone.stop(); haptic.stop() }
    NonGraphicalFeedback {
        id: haptic
        event: "feedback_press"
        onStatusChanged: {
            if(status===NonGraphicalFeedback.Playing)feedback.hapticsPlayed++
            else if(status===NonGraphicalFeedback.Failed)feedback.hapticFailures++
        }
    }
    Connections { target: appNavigation; onCurrentTabChanged: feedback.stop() }
    Connections { target: Qt.application; onStateChanged: if(Qt.application.state!==Qt.ApplicationActive)feedback.stop() }
    Component.onDestruction: stop()
}
