import QtQuick 2.5
import QtQuick.Controls.Universal
import QtQuick.Layouts
import App 1.0

GroupBox {
    id: stepMotor
    property int tbwidth: 80
    padding: 4
    font.pixelSize: 12
    title: qsTr("Z-STAGE CONTROL")

    GridLayout {
        id: zStage
        property string motorTargetPos
        rows: 3
        columns: 2
        anchors.fill: parent
        Label {
            text: qsTr("Target elevation [mm]")
        }

        ComboBox {
            id: motorPositionCombobox
            Layout.preferredWidth: stepMotor.tbwidth
            Layout.leftMargin: 28
            model: ["1.0", "2.0", "3.0", "4.0", "5.0", "6.0", "7.0", "8.0", "9.0", "10.0", "11.0", "12.0", "13.0", "14.0", "15.0", "16.0", "17.0", "18.0", "19.0", "20.0", "21.0"]
            onActivated: {
                zStage.motorTargetPos = motorPositionCombobox.currentText;
            }
        }

        Label {
            text: qsTr("Current elevation [mm]")
        }
        TextField {
            id: currentPos
            text: qsTr("cp")
            Layout.preferredWidth: stepMotor.tbwidth
            Layout.leftMargin: 28
        }

        Button {
            id: btnHome
            text: "HOME"
            Layout.preferredWidth: stepMotor.tbwidth
            onClicked: {
                console.log("HOME Button clicked");
                appContext.stepperControllerService.moveHome();
            }
        }
        Button {
            id: btnMove
            text: "MOVE"
            Layout.preferredWidth: stepMotor.tbwidth
            Layout.leftMargin: 26
            onClicked: {
                console.log("MOVE Button clicked");
                appContext.stepperControllerService.moveToPosition(Number(zStage.motorTargetPos));
            }
        }
    }
}
