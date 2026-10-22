import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: root

    readonly property bool isReallyVisible: visible && parent && parent.visible

    // Dynamic states/status properties
    property string calibrationStatus: "None" // "Pass", "Fail", or "None"
    property string actionState: "Run"        // "Run", "Running", or "Done"

    signal runRequested()
    signal doneRequested()

    background: Rectangle {
        color: "#FFFFFF"
    }

    // Main Layout
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40 // Outer padding
        spacing: 24

        // Top Header Section with Status Badge
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 50

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    text: "Instrument Axis Calibration"
                    font.pixelSize: 16
                    font.bold: true
                    color: "#111111"
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: "TOOL-XXXX"
                    font.pixelSize: 13
                    color: "#111111"
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // Top-Right Status Badge
            Rectangle {
                id: statusBadge
                width: 160
                height: 45
                anchors.top: parent.top
                anchors.right: parent.right

                color: "#8cc63f"
                radius: height / 2
                border.color: "#333333"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Status: " + root.calibrationStatus
                    font.pixelSize: 12
                    color: "#000000"
                }
            }
        }

        // Camera Panels Area (NFOV Row Only)
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 30

                Item {
                    id: nfovLeftContainer
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    CameraControl {
                        id: nfovLeftBox
                        anchors.centerIn: parent
                        property real maxWidth: parent.width * 0.85
                        property real maxHeight: parent.height * 0.85

                        width: Math.min(maxWidth, maxHeight * (16 / 9))
                        height: width * (9 / 16)
                        imageModel: appContext.instrumentAxisProcessor.leftOverlay
                    }
                }

                Item {
                    id: nfovRightContainer
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    CameraControl {
                        id: nfovRightBox
                        anchors.centerIn: parent
                        property real maxWidth: parent.width * 0.85
                        property real maxHeight: parent.height * 0.85

                        width: Math.min(maxWidth, maxHeight * (16 / 9))
                        height: width * (9 / 16)
                        imageModel: appContext.instrumentAxisProcessor.rightOverlay
                    }
                }
            }
        }

        // Action Button (Run / Running / Done)
        Button {
            id: actionBtn
            text: root.actionState
            enabled: root.actionState !== "Running"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Layout.preferredHeight: 45

            background: Rectangle {
                color: {
                    if (!actionBtn.enabled) return "#DCE775"
                    return actionBtn.down ? "#7CB342" : "#8BC34A"
                }
                border.color: "#333333"
                border.width: 1
                radius: 20
            }

            contentItem: Text {
                text: actionBtn.text
                font.pixelSize: 15
                font.bold: true
                color: "#111111"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                  nav.push(
                    "qrc:/qt/qml/App/qml/pages/HomePage.qml")  
            }
        }
    }
}