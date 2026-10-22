import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: root

    readonly property var wfovProcessor:
        appContext ? appContext.wfovContrastProcessor : null

    readonly property bool isReallyVisible:
        visible && parent && parent.visible

    readonly property bool framesComplete:
        wfovProcessor &&
        wfovProcessor.validFrameCount >= wfovProcessor.requiredFrameCount

    readonly property color progressColor:
        !wfovProcessor
            ? "#C62828"
            : framesComplete
                ? "#2E7D32"   // green
                : "#F9A825"   // yellow

    onIsReallyVisibleChanged: {
        if (!wfovProcessor)
            return

        if (isReallyVisible) {
            appContext.cameraOffsetProcessor.clearOverlays()
            appContext.cameraOffsetProcessor.imageService.startWideVideo()
            wfovProcessor.resetMeasurement()
        } else {
            appContext.cameraOffsetProcessor.imageService.stopWideVideo()
            wfovProcessor.resetMeasurement()
        }
    }

    Connections {
        target: appContext.cameraOffsetProcessor.imageService

        function onImagesReady(imageL, imageR) {
            imgContainer.setImageVariant(imageL)
        }
    }

    background: Rectangle {
        color: "#FFFFFF"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 30
        spacing: 16

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4

            Text {
                text: "WFOV Assembly Calibration"
                font.pixelSize: 18
                font.bold: true
                color: "#111111"
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "TOOL-XXXX"
                font.pixelSize: 14
                color: "#111111"
                Layout.alignment: Qt.AlignHCenter
            }
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter

            width: 350
            height: 220

            color: "#F8F8F8"
            border.color: "#333333"
            border.width: 1

            PixmapImage {
                id: imgContainer

                anchors.fill: parent
                anchors.margins: 1

                OverlayCanvas {
                    id: overlay

                    z: 10
                    anchors.fill: parent

                    model: appContext.cameraOffsetProcessor.imageModel
                    imageWidth: imgContainer.sourceImageWidth
                    imageHeight: imgContainer.sourceImageHeight
                }

                Text {
                    anchors.centerIn: parent

                    text: "WFOV"

                    font.pixelSize: 16
                    color: "#999999"
                    z: 5

                    visible:
                        imgContainer.sourceImageWidth <= 0 ||
                        imgContainer.sourceImageHeight <= 0
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter

            text: wfovProcessor
                  ? wfovProcessor.statusText
                  : "WFOV processor unavailable"

            font.pixelSize: 13
            color: "#555555"
        }

        Text {
            Layout.alignment: Qt.AlignHCenter

            text: wfovProcessor
                  ? "Valid frames: "
                    + wfovProcessor.validFrameCount
                    + " / "
                    + wfovProcessor.requiredFrameCount
                  : ""

            font.pixelSize: 12
            font.bold: true
            color: progressColor
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter

            width: 330
            height: 125

            color: "#F8F8F8"
            border.width: 1
            radius: 4

            border.color: progressColor

            Column {
                anchors.centerIn: parent
                spacing: 5

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter

                    text: "Average Linear Contrast: "
                          + (
                              wfovProcessor
                              ? wfovProcessor.linearContrastPercent.toFixed(2)
                              : "0.00"
                            )
                          + "%"

                    font.pixelSize: 16
                    font.bold: true

                    color: progressColor
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter

                    text: "Average Global Contrast: "
                          + (
                              wfovProcessor
                              ? wfovProcessor.globalContrastPercent.toFixed(2)
                              : "0.00"
                            )
                          + "%"

                    font.pixelSize: 13
                    color: "#333333"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter

                    text: "Linear Left: "
                          + (
                              wfovProcessor
                              ? wfovProcessor.leftLinearContrastPercent.toFixed(2)
                              : "0.00"
                            )
                          + "%    Right: "
                          + (
                              wfovProcessor
                              ? wfovProcessor.rightLinearContrastPercent.toFixed(2)
                              : "0.00"
                            )
                          + "%"

                    font.pixelSize: 12
                    color: "#333333"
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter

            text:
                !wfovProcessor
                    ? "Press Calculate Contrast"
                    : framesComplete
                        ? "Measured (" + wfovProcessor.validFrameCount + "/" + wfovProcessor.requiredFrameCount + " frames)"
                        : "Collecting frames (" + wfovProcessor.validFrameCount + "/" + wfovProcessor.requiredFrameCount + ")"

            font.pixelSize: 12

            color: progressColor
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            Button {
                id: calculateButton

                text:
                    wfovProcessor &&
                    wfovProcessor.measuring
                    ? "Collecting..."
                    : "Calculate Contrast"

                enabled:
                    wfovProcessor !== null &&
                    !wfovProcessor.measuring

                Layout.preferredWidth: 175
                Layout.preferredHeight: 40

                onClicked: {
                    wfovProcessor.startMeasurement()
                }
            }

            Button {
                id: confirmButton

                text:
                    wfovProcessor &&
                    wfovProcessor.saved
                    ? "Saved"
                    : "Confirm & Save"

                enabled:
                    wfovProcessor !== null &&
                    wfovProcessor.measurementReady &&
                    !wfovProcessor.saved

                Layout.preferredWidth: 175
                Layout.preferredHeight: 40

                onClicked: {
                    wfovProcessor.confirmAndSave()
                }
            }
        }

        Button {
            id: doneButton

            text: "Done"

            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Layout.preferredHeight: 40

            background: Rectangle {
                color: doneButton.down
                       ? "#7CB342"
                       : "#8BC34A"

                border.color: "#333333"
                border.width: 1
                radius: 20
            }

            contentItem: Text {
                text: doneButton.text

                font.pixelSize: 15
                font.bold: true
                color: "#111111"

                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                nav.pop()
            }
        }
    }
}