import QtQuick 2.15
import QtQuick.Controls.Universal
import QtQuick.Layouts

import App 1.0

GroupBox {
    id: htrigger

    padding: 4
    font.pixelSize: 12
    property int tbwidth: 80
    property int tbheight: 80

    Layout.topMargin: 10
    title: qsTr("HARDWARE TRIGGER")

    GridLayout {
        id: trigger
        rows: 3
        columns: 2
        columnSpacing: 0

        Label {
            text: qsTr("Frequency [Hz]")
        }
        TextField {
            id: frequency
            text: appContext.triggerSettingsService.frequency
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            Layout.row: 0
            Layout.column: 1
            Layout.preferredWidth: htrigger.tbwidth
            Layout.leftMargin: 77
            background: Rectangle {
                border.color: "gray"
            }
        }

        Label {
            text: qsTr("Duration [msec]")
            Layout.row: 1
            Layout.column: 0
        }
        TextField {
            id: duration
            text: appContext.triggerSettingsService.duration
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            Layout.row: 1
            Layout.column: 1
            Layout.preferredWidth: htrigger.tbwidth
            Layout.leftMargin: 77
            background: Rectangle {
                border.color: "gray"
            }
        }

        Connections {
            target: appContext.triggerSettingsService

            function onApplySucceeded(message) {
                console.log("Trigger apply success:", message)
            }

            function onApplyFailed(message) {
                console.log("Trigger apply failed:", message)
            }
        }

        Button {
            id: triggerState
            checkable: true
            text: checked ? "ON" : "OFF"
            Layout.row: 2
            Layout.column: 1
            Layout.preferredWidth: htrigger.tbwidth
            Layout.alignment: Qt.AlignRight

            onClicked: {
                let freq = parseInt(frequency.text, 10)
                let dur = parseInt(duration.text, 10)

                if (isNaN(freq)) freq = 0
                if (isNaN(dur)) dur = 0

                freq = Math.max(0, freq)
                dur = Math.max(0, dur)

                if (!checked) {
                    freq = 0
                    dur = 0
                }

                appContext.triggerSettingsService.frequency = freq
                appContext.triggerSettingsService.duration = dur

                console.log("Trigger frequency set to:", appContext.triggerSettingsService.frequency)
                console.log("Trigger duration set to:", appContext.triggerSettingsService.duration)

                appContext.triggerSettingsService.apply()
            }
        }
    }
}
