import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
Page {
    id: root

    property var processor: appContext.photometricProcessor
    readonly property bool isReallyVisible: visible && parent && parent.visible

    // Dynamic states/status properties
    property string calibrationStatus: "None" // "Pass", "Fail", or "None"
    property string actionState: "Run"        // "Run", "Running", or "Next"

    signal runRequested()
    signal nextRequested()

    background: Rectangle {
        color: "#FFFFFF"
    }

    onVisibleChanged: {
        if (!visible && processor)
            processor.clearOverlays();
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
                    text: "Photometric Calibration"
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

                color: {
                    switch (root.calibrationStatus) {
                    case "Pass":
                        return "#8cc63f"
                    case "Fail":
                        return "#E57373"
                    default:
                        return "#D3D3D3"
                    }
                }
                radius: height / 2
                border.color: "#333333"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: root.calibrationStatus
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
                        imageModel: appContext.photometricProcessor.leftOverlay
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
                        imageModel: appContext.photometricProcessor.rightOverlay
                    }
                }
            }
        }

        ProgressBar {
            id: progressBar
            value: processor.progress
            Layout.fillWidth: true
            Layout.preferredHeight: 45
            Layout.leftMargin: 20
            Layout.rightMargin: 20

            background: Rectangle {
                implicitWidth: 200
                implicitHeight: 45
                color: "#D3D3D3"
                radius: height / 2
                border.color: "#333333"
                border.width: 1
            }

            contentItem: Item {
                implicitWidth: 200
                implicitHeight: 45

                Rectangle {
                    width: progressBar.visualPosition * parent.width
                    height: parent.height
                    color: "#FFC107"
                    radius: parent.height / 2
                    border.color: "#333333"
                    border.width: 1
                }

                Text {
                    anchors.centerIn: parent
                    text: "Progress (" + Math.round(progressBar.value * 100) + "%)"
                    font.pixelSize: 14
                    font.bold: true
                    color: "#111111"
                }
            }
        }

         Text {
             id: stMessage

             Layout.fillWidth: true
             Layout.preferredHeight: 25

             text: processor.statusMessage
             horizontalAlignment: Text.AlignHCenter
             verticalAlignment: Text.AlignVCenter

             font.pixelSize: 13
             color: "#111111"
             elide: Text.ElideRight
         }

        // Action Button (Run / Running / Next)
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
                if (root.actionState === "Run" ) {
                    root.actionState = "Running";
                    root.processor.execute("");
                    root.runRequested();

                    console.log("Calibrate clicked");
                }
                else if (root.actionState === "Next" ) {
                    root.actionState = "Done";
                    root.nextRequested();

                    nav.push(
                        "qrc:/qt/qml/App/qml/pages/StereoCalibration3mmPage.qml"
                    );
                }
            }
        }
        // Image acquisition
        Connections {
            target: processor.imageService

            function onImagesReady(imageL, imageR) {
                nfovLeftBox.setImage(imageL)
                nfovRightBox.setImage(imageR)

                console.log(
                    "Photometric: Images received:",
                    imageL.size,
                    imageR.size
                )
            }
        }

        //Update calibration result/status
        Connections {
            target: processor
            function onResultStatusTextChanged() {
                console.log("(photometric) resultStatusTextChanged:",
                     processor.resultStatusText)
                if (processor.resultStatusText === "Pass" &&
                    root.actionState === "Running") {
                    root.calibrationStatus = "Pass"
                    root.actionState = "Next"
                }
                else if (processor.resultStatusText === "Fail" ){
                    root.calibrationStatus = "Fail"
                    root.actionState = "Run"
                    //processor.resetProgress()
                }
            }
        }
    }
}