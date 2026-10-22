import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    signal navigateRequested(string targetScreen)

    background: Rectangle {
        color: "#ffffff"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 30

        Label {
            text: "Head Assembly"
            font.pixelSize: 20
            font.bold: true
            color: "#222222"
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 10
            Layout.bottomMargin: 30
        }

        // Top row
        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 60

            Item { Layout.fillWidth: true }

            NavButton {
                text: "System Calibration"
                Layout.preferredWidth: 220
                onClicked: nav.push(
                    //"qrc:/qt/qml/App/qml/pages/OffsetCalibrationPage.qml")
                               "qrc:/qt/qml/App/qml/pages/StereoCalibration3mmPage.qml")
            }

            NavButton {
                text: "UV Irradiance Calibration"
                Layout.preferredWidth: 220
                onClicked: nav.push(
                    "qrc:/qt/qml/App/qml/pages/UVIrradianceCalibrationPage.qml")                
            }

            NavButton {
                text: "Depth Sensor Calibration"
                Layout.preferredWidth: 220
                onClicked: nav.push(
                    "qrc:/qt/qml/App/qml/pages/DepthSensorCalibrationPage.qml")                
            }

            Item { Layout.fillWidth: true }
        }

        // Space between rows
        Item {
            Layout.preferredHeight: 60
        }

        // Bottom row
        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 70

            Item { Layout.fillWidth: true }

            NavButton {
                text: "UV Uniformity Check"
                onClicked: root.navigateRequested(text)
                Layout.preferredWidth: 220
            }

            NavButton {
                text: "Depth of Field Check"
                onClicked: root.navigateRequested(text)
                Layout.preferredWidth: 220
            }
            NavButton {
                text: "DMD Calibration"
                Layout.preferredWidth: 220
                onClicked: nav.push(
                    "qrc:/qt/qml/App/qml/pages/DmdCalibrationPage.qml")
            }
            Item { Layout.fillWidth: true }
        }

        // Space between rows
        Item {
            Layout.preferredHeight: 60
        }

        // Bottom row
        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 70

            Item { Layout.fillWidth: true }

            NavButton {
                text: "Stereo Cal"
                Layout.preferredWidth: 220
                onClicked: nav.push(
                    "qrc:/qt/qml/App/qml/pages/StereoCalibration3mmPage.qml")

            }

            NavButton {
                text: "Stereo Check"
                Layout.preferredWidth: 220
                onClicked: nav.push(
                    "qrc:/qt/qml/App/qml/pages/StereoCalibration4mmPage.qml")
            }
            NavButton {
                text: "DMD Check"
                Layout.preferredWidth: 220
                onClicked: nav.push(
                    "qrc:/qt/qml/App/qml/pages/DmdCalibrationCheckPage.qml")
            }
            Item { Layout.fillWidth: true }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
