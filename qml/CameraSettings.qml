import QtQuick 2.15
import QtQuick.Controls.Universal
import QtQuick.Layouts
import App 1.0

GroupBox {
    id: gbCameraSettings
    title: qsTr("CAMERA SETTINGS")
    padding: 4
    font.pixelSize: 12

    GridLayout {
        id: cameraSettings
        rows: 4
        columns: 3
        columnSpacing: 6
        anchors.fill: parent

        Label { text: qsTr("Left");  Layout.row: 0; Layout.column: 1 }
        Label { text: qsTr("Right"); Layout.row: 0; Layout.column: 2 }

        Label {
            text: qsTr("Camera \ngain [dB]")
            Layout.row: 1
            Layout.column: 0
        }

        CustomTextField {
            id: camGainLeft
            text: appContext.cameraSettingsService.leftGain
            Layout.row: 1
            Layout.column: 1
        }
        CustomTextField {
            id: camGainRight
            text: appContext.cameraSettingsService.rightGain
            Layout.row: 1
            Layout.column: 2
        }

        Label {
            text: qsTr("Camera \nexposure [usec]")
            Layout.row: 2
            Layout.column: 0
        }

        // Single exposure box used for both cameras
        CustomTextField {
            id: camExposure
            text: appContext.cameraSettingsService.leftExposureUs
            Layout.row: 2
            Layout.column: 1
            Layout.columnSpan: 2
        }

        Button {
            id: setCameraParams
            text: "SET"
            Layout.preferredWidth: 80
            Layout.row: 3
            Layout.columnSpan: 3
            Layout.leftMargin: 160
            enabled: !appContext.cameraSettingsService.busy

            onClicked: {
                let leftGain = parseInt(camGainLeft.text, 10)
                let rightGain = parseInt(camGainRight.text, 10)
                let exp = parseInt(camExposure.text, 10)

                if (isNaN(leftGain)) leftGain = 0
                if (isNaN(rightGain)) rightGain = 0
                if (isNaN(exp)) exp = 0

                leftGain = Math.max(0, leftGain)
                rightGain = Math.max(0, rightGain)
                exp = Math.max(0, exp)

                appContext.cameraSettingsService.leftGain = leftGain
                appContext.cameraSettingsService.rightGain = rightGain

                // same exposure to both
                appContext.cameraSettingsService.leftExposureUs = exp
                appContext.cameraSettingsService.rightExposureUs = exp

                appContext.cameraSettingsService.apply()

                console.log("LEFT GAIN:", appContext.cameraSettingsService.leftGain)
                console.log("RIGHT GAIN:", appContext.cameraSettingsService.rightGain)
                console.log("EXPOSURE (both):", exp)
            }
        }
    }

    Connections {
        target: appContext.cameraSettingsService
        function onApplySucceeded(message) { console.log("Camera settings success:", message) }
        function onApplyFailed(message) { console.log("Camera settings failed:", message) }
    }
}