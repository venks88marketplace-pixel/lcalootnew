import QtQuick 2.15
import QtQuick.Controls.Universal
import QtQuick.Layouts
import App 1.0

GroupBox {
    id: uvControl
    property int tbwidth: 80
    Layout.topMargin: 10
    padding: 4
    font.pixelSize: 12
    title: qsTr("UV CONTROL")

    property bool uvState: false

    GridLayout {
        id: uvOnOff
        rows: 2
        columns: 2

        Label {
            text: qsTr("UV irradiance \n [uW/cm^2]")
            Layout.row: 0
            Layout.column: 0
        }

        TextField {
            id: uvIrradiance
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            Layout.row: 0
            Layout.column: 1
            Layout.leftMargin: 83
            Layout.preferredWidth: uvControl.tbwidth
            text: appContext.uvSettingsService.power
        }

        Button {
            id: uvOnOffBtn
            checkable: true
            text: checked ? "UV ON" : "UV OFF"
            Layout.row: 1
            Layout.column: 1
            Layout.alignment: Qt.AlignRight
            Layout.preferredWidth: 80
            enabled: !appContext.uvSettingsService.busy

            onClicked: {
                let pow = parseInt(uvIrradiance.text, 10)
                if (isNaN(pow)) pow = 0
                pow = Math.max(0, pow)

                appContext.uvSettingsService.power = pow
                appContext.uvSettingsService.setEnabled(checked)

                uvState = checked
                console.log("UV POWER(dac_counts):", appContext.uvSettingsService.power)
            }
        }
    }

    Connections {
        target: appContext.uvSettingsService
        function onApplySucceeded(message) { console.log("UV success:", message) }
        function onApplyFailed(message) { console.log("UV failed:", message) }
    }
}