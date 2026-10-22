import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root

    // Define custom properties to allow external customization if needed
    property color normalColor: "#999999"
    property color hoverColor: "#aaaaaa"
    property color pressColor: "#888888"
    property color textColor: "#1a1a1a"

    Layout.fillWidth: true
    Layout.maximumWidth: 300
    implicitWidth: 280
    implicitHeight: 75

    contentItem: Text {
        text: root.text
        font.pixelSize: 15
        font.weight: Font.Medium
        font.family: "sans-serif"
        color: root.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }

    background: Rectangle {
        color: root.pressed ? root.pressColor : (root.hovered ? root.hoverColor : root.normalColor)
        radius: 22
        border.color: "#333333"
        border.width: 1.5

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }
}