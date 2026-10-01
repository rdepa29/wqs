import Quickshell
import Quickshell.Io
import QtQuick

ShellRoot {
    id: root

    IpcHandler {
        target: "example"

        function handle(message: string): string {
            return "received: " + message
        }
    }

    PanelWindow {
        id: bar

        anchors {
            top: true
            left: true
            right: true
        }
        // Reserving 36px of the top edge for the bar.
        exclusiveZone: 36
        implicitHeight: 36
        color: "#1e1e2e"

        property string currentTime: Qt.formatTime(new Date())

        Timer {
            interval: 1000
            running: true
            repeat: true
            onTriggered: bar.currentTime = Qt.formatTime(new Date())
        }

        Row {
            anchors.centerIn: parent
            spacing: 24

            Text {
                text: bar.currentTime
                color: "#cdd6f4"
                font.pixelSize: 14
            }

            Text {
                text: "wqs"
                color: "#f9e2af"
                font.pixelSize: 14
            }
        }
    }

    FloatingWindow {
        id: example

        visible: false
        implicitWidth: 260
        implicitHeight: 160
        color: "#313244"

        Text {
            anchors.centerIn: parent
            text: "hello wqs"
            color: "#cdd6f4"
            font.pixelSize: 16
        }
    }
}
