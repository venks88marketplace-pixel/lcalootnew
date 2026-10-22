import QtQuick
import QtQuick.Controls.Universal
import QtQuick.Layouts
import App 1.0

Item {
    id: root
    implicitWidth: 990
    implicitHeight: 856

    visible: true
    required property int imageControlWidth
    required property int imageControlHeight

    property var processor: appContext.cameraOffsetProcessor
    required property var overlayModelLeft
    required property var overlayModelRight

    Component.onCompleted: {
        console.log("ImageControl appContext =", appContext);
        console.log("ImageControl imageService =", appContext ? appContext.imageService : "undefined");
    }

    GridLayout {
        columns: 3
        rows: 3
        columnSpacing: 16
        anchors.fill: parent
        anchors.margins: 20
        Label {
            text: "Left camera"
            Layout.column: 0
        }
        Label {
            text: "Right camera"
            Layout.column: 1
        }

        PixmapImage {
            id: imLeft
            Layout.preferredWidth: root.imageControlWidth
            Layout.preferredHeight: root.imageControlHeight
            Layout.row: 1
            OverlayCanvas {
                id: leftOverlay
                z: 10
                anchors.fill: parent
                model: root.overlayModelLeft
                imageWidth: imLeft.sourceImageWidth
                imageHeight: imLeft.sourceImageHeight
            }
        }
        Connections {
            target: imLeft
            function onSourceImageSizeChanged() {
                console.log("PixmapImage source changed:", imLeft.sourceImageWidth, imLeft.sourceImageHeight);
            }
        }

        PixmapImage {
            id: imRight
            Layout.row: 1
            Layout.column: 1
            Layout.preferredWidth: root.imageControlWidth
            Layout.preferredHeight: root.imageControlHeight
            OverlayCanvas {
                id: rightOverlay
                z: 10
                anchors.fill: parent
                model: root.overlayModelRight
                imageWidth: imRight.sourceImageWidth
                imageHeight: imRight.sourceImageHeight
            }
        }
        Connections {
            target: imRight
            function onSourceImageSizeChanged() {
                console.log("PixmapImage source changed:", imRight.sourceImageWidth, imRight.sourceImageHeight);
            }
        }
    }
    Connections {
        target: root.processor.imageService
        function onImagesReady(imageL, imageR) {
            imLeft.setImage(imageL);
            imRight.setImage(imageR);
            console.log("item size:", imLeft.width, imLeft.height);
        }
    }
}
