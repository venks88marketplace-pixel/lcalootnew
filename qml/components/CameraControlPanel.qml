import QtQuick 2.15

Item {
    id: root

    // Public Properties
    property alias mtfText: mtfLabel.text
    property string placeholderText: ""
    property int imageControlWidth: displayBox.width
    property int imageControlHeight: displayBox.height
    required property var imageModel

    // Signals
    signal panRequested(string direction)

    // Outer container allowing full dynamic scaling
    Item {
        anchors.fill: parent

        // Max-bounded container that dynamically resizes with window
        Item {
            id: maxContainer
            anchors.centerIn: parent

            width: Math.min(parent.width, 480)
            height: Math.min(parent.height, 400)

            ChevronButton {
                id: upBtn
                direction: "up"
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: panRequested("up")
            }

            Rectangle {
                id: mtfBox
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: displayBox.width
                height: 28
                color: "transparent"
                border.color: "#333333"
                border.width: 1

                Text {
                    id: mtfLabel
                    anchors.centerIn: parent
                    font.pixelSize: 13
                    color: "#111111"
                }
            }

            ChevronButton {
                id: downBtn
                direction: "down"
                anchors.bottom: mtfBox.top
                anchors.bottomMargin: 4
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: panRequested("down")
            }

            ChevronButton {
                id: leftBtn
                direction: "left"
                anchors.left: parent.left
                anchors.verticalCenter: displayBox.verticalCenter
                onClicked: panRequested("left")
            }

            ChevronButton {
                id: rightBtn
                direction: "right"
                anchors.right: parent.right
                anchors.verticalCenter: displayBox.verticalCenter
                onClicked: panRequested("right")
            }

            CameraControl {
                id: displayBox
                anchors.top: upBtn.bottom
                anchors.bottom: downBtn.top
                anchors.left: leftBtn.right
                anchors.right: rightBtn.left
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                anchors.leftMargin: 4
                anchors.rightMargin: 4

                // Required property forwarded to inner component
                imageModel: root.imageModel
            }
        }
    }
}