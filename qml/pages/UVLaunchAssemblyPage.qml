import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: root

    readonly property bool isReallyVisible: visible && parent && parent.visible

    // Aspect ratios
    readonly property real mainAspect:
        (imgContainer.sourceImageWidth > 0 && imgContainer.sourceImageHeight > 0)
            ? imgContainer.sourceImageWidth / imgContainer.sourceImageHeight
            : 2016.0 / 1452.0
    readonly property real cropAspect: 1.0 / 3.0         // MTF strip (w/h)
    readonly property int viewSpacing: 12

    property bool holdingCamera: false

    function takeCamera() {
        if (holdingCamera) return;
        holdingCamera = true;
        appContext.uvAlignmentProcessor.clearOverlays();
        appContext.baslerImageService.acquireCamera();
    }

    function dropCamera() {
        if (!holdingCamera) return;
        holdingCamera = false;
        appContext.baslerImageService.releaseCamera();
    }

    onIsReallyVisibleChanged: {
        if (isReallyVisible) takeCamera();
        else dropCamera();
    }

    Component.onCompleted: {
        if (isReallyVisible) takeCamera();
    }

    Component.onDestruction: dropCamera()

    background: Rectangle {
        color: "#FFFFFF"
    }

    // Camera frames -> used for analysis
    Connections {
        target: appContext.baslerImageService
        function onImageReady(image) {
            if (root.isReallyVisible)
                appContext.uvAlignmentProcessor.processFrame(image)
        }
    }

    // Analysis results -> used for display
    Connections {
        target: appContext.uvAlignmentProcessor
        function onDisplayFrameReady(image) {
            imgContainer.setImage(image)
        }
        function onCropFrameReady(image) {
            cropContainer.setImage(image)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        // Header
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 2

            Text {
                text: "UV Launch Assembly Calibration"
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

        // Views area: sizes are computed live
        Item {
            id: viewsArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200

            readonly property real mainW: Math.max(
                100,
                Math.min((width - root.viewSpacing) / (1.0 + root.cropAspect / root.mainAspect),
                         height * root.mainAspect))
            readonly property real mainH: mainW / root.mainAspect
            readonly property real cropH: mainH
            readonly property real cropW: cropH * root.cropAspect

            Row {
                anchors.centerIn: parent
                spacing: root.viewSpacing

                // Main view
                Rectangle {
                    width: viewsArea.mainW
                    height: viewsArea.mainH
                    color: "#F8F8F8"
                    border.color: "#333333"
                    border.width: 1

                    PixmapImage {
                        id: imgContainer
                        anchors.fill: parent
                        anchors.margins: 1
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "Waiting for camera..."
                        font.pixelSize: 16
                        color: "#999999"
                        visible: imgContainer.sourceImageWidth <= 0
                    }
                }

                // MTF edge strip
                Column {
                    spacing: 4

                    Text {
                        text: "Edge (MTF strip)"
                        font.pixelSize: 11
                        color: "#111111"
                        width: viewsArea.cropW
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        width: viewsArea.cropW
                        height: viewsArea.cropH - 15
                        color: "#000000"
                        border.color: "#333333"
                        border.width: 1

                        PixmapImage {
                            id: cropContainer
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                }
            }
        }

        // Informational box
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 320
            Layout.preferredHeight: 80
            color: "#F0F0F0"
            border.color: "#333333"
            border.width: 1

            GridLayout {
                anchors.fill: parent
                anchors.margins: 8
                columns: 2
                columnSpacing: 16
                rowSpacing: 2

                Text { text: "Centroid:"; font.pixelSize: 12; color: "#555555" }
                Text {
                    text: appContext.uvAlignmentProcessor.centroidX.toFixed(0) + ", "
                          + appContext.uvAlignmentProcessor.centroidY.toFixed(0)
                    font.pixelSize: 12; color: "#111111"
                }

                Text { text: "MTF @ 1.50 lp/mm:"; font.pixelSize: 12; color: "#555555" }
                Text {
                    text: appContext.uvAlignmentProcessor.mtf.toFixed(2)
                    font.pixelSize: 12; color: "#111111"
                }

                Text {
                    text: "Uniformity RMS (r=" + appContext.uvAlignmentProcessor.uniformityRadius + " px):"
                    font.pixelSize: 12; color: "#555555"
                }
                Text {
                    text: appContext.uvAlignmentProcessor.uniformityRMS.toFixed(2) + " %"
                    font.pixelSize: 12; color: "#111111"
                }

                Text { text: "Uniformity PkPk:"; font.pixelSize: 12; color: "#555555" }
                Text {
                    text: appContext.uvAlignmentProcessor.uniformityPkPk.toFixed(2) + " %"
                    font.pixelSize: 12; color: "#111111"
                }

            }
        }

        // Control buttons
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 16

            Button {
                text: "False Color: " + (appContext.uvAlignmentProcessor.applyFalseColor ? "ON" : "OFF")
                Layout.preferredWidth: 150
                Layout.preferredHeight: 40
                onClicked: appContext.uvAlignmentProcessor.toggleFalseColor()
            }

            Button {
                text: "Mean Filter: " + (appContext.uvAlignmentProcessor.applyMeanFilter ? "ON" : "OFF")
                Layout.preferredWidth: 150
                Layout.preferredHeight: 40
                onClicked: appContext.uvAlignmentProcessor.toggleMeanFilter()
            }

            Button {
                text: "Save Data"
                Layout.preferredWidth: 150
                Layout.preferredHeight: 40
                onClicked: {
                    var folder = appContext.uvAlignmentProcessor.saveData();
                    console.log("Data saved to: " + folder);
                }
            }
        }

        // Next button
        Button {
            id: nextBtn
            text: "Next"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Layout.preferredHeight: 40

            background: Rectangle {
                color: nextBtn.down ? "#7CB342" : "#8BC34A"
                border.color: "#333333"
                border.width: 1
                radius: 20
            }

            contentItem: Text {
                text: nextBtn.text
                font.pixelSize: 15
                font.bold: true
                color: "#111111"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                nav.push("qrc:/qt/qml/App/qml/pages/UVBlockAssemblyPage.qml")
            }
        }
    }
}
