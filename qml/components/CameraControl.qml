import QtQuick 2.15
import App 1.0

Rectangle {
    id: root

    // Public Properties
    property alias sourceImageWidth: imgContainer.sourceImageWidth
    property alias sourceImageHeight: imgContainer.sourceImageHeight
    required property var imageModel

    // Default Visual Styling
    color: "#F8F8F8"
    border.color: "#333333"
    border.width: 1

    function setImage(image) {
        imgContainer.setImage(image)
    }

    PixmapImage {
        id: imgContainer
        anchors.fill: parent
        anchors.margins: root.border.width

        OverlayCanvas {
            id: overlay
            z: 10
            anchors.fill: parent
            model: root.imageModel
            imageWidth: imgContainer.sourceImageWidth
            imageHeight: imgContainer.sourceImageHeight
        }
    }
}