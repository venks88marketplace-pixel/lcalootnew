import QtQuick 2.15
import QtQuick.Controls.Universal
import QtQuick.Layouts

import App 1.0

GroupBox {
    id: root
    property int tbwidth: 80
    property int tbheight: 80
    property bool nirledState: false
    padding: 4
    font.pixelSize: 12
    Component.onCompleted: {
        console.log("NearLEDs nirledsettingsservice =", appContext.nirledSettingsService ? appContext.nirledSettingsService.levelTop : "undefined");
    }
    title: qsTr("NIR LEDs")

    Connections {
        target: appContext.nirledSettingsService

        function onApplySucceeded(message) {
            console.log("NIR apply success:", message)
        }

        function onApplyFailed(message) {
            console.log("NIR apply failed:", message)
        }
    }

    GridLayout {
        id: ledControls1
        rows: 5
        columns: 2
        columnSpacing: 0
        Label {
            text: qsTr("Power level Top  [%]")
        }
        TextField {
            id: ledLevelTop
            text: appContext.nirledSettingsService.levelTop
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            Layout.row: 0
            Layout.column: 1
            Layout.preferredWidth: root.tbwidth
            Layout.leftMargin: 52
            background: Rectangle {
                border.color: "gray"
            }
        }

        Label {
            text: qsTr("Power level Bot [%]")
            Layout.row: 1
            Layout.column: 0
        }
        TextField {
            id: ledLevelBot
            text: appContext.nirledSettingsService.levelBot
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            Layout.row: 1
            Layout.column: 1
            Layout.preferredWidth: root.tbwidth
            Layout.leftMargin: 52
            background: Rectangle {
                border.color: "gray"
            }
        }

        Button {
            id: setLeds
            checkable: true
            text: checked ? "ON" : "OFF"
            Layout.row: 4
            Layout.column: 1
            Layout.preferredWidth: root.tbwidth
            Layout.alignment: Qt.AlignRight

            onClicked: {
                let tInt = parseInt(ledLevelTop.text, 10)
                let bInt = parseInt(ledLevelBot.text, 10)

                if (isNaN(tInt)) tInt = 0
                if (isNaN(bInt)) bInt = 0

                // Clamp 0..100
                tInt = Math.max(0, Math.min(100, tInt))
                bInt = Math.max(0, Math.min(100, bInt))

                if (checked) {
                    appContext.nirledSettingsService.levelTop = tInt
                    appContext.nirledSettingsService.levelBot = bInt
                    nirledState = true
                } else {
                    appContext.nirledSettingsService.levelTop = 0
                    appContext.nirledSettingsService.levelBot = 0
                    nirledState = false
                }

                console.log("NIRLED TOP:", appContext.nirledSettingsService.levelTop)
                console.log("NIRLED BOT:", appContext.nirledSettingsService.levelBot)

                // Send protobuf request
                appContext.nirledSettingsService.apply()
            }
        }
    }
}
