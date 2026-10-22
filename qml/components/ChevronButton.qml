import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: btn
    property string direction: "up"

    implicitWidth: 28
    implicitHeight: 28

    background: Rectangle {
        color: btn.pressed ? "#E0E0E0" : (btn.hovered ? "#F2F2F2" : "transparent")
        radius: 4
    }

    contentItem: Canvas {
        id: canvas
        anchors.fill: parent

        // Trigger repaints automatically when button states change
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        // Direct property listeners replacing Connections
        Connections {
            target: btn
            onPressedChanged: canvas.requestPaint()
            onHoveredChanged: canvas.requestPaint()
        }

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.lineWidth = 1.5;
            ctx.strokeStyle = "#111111";
            ctx.fillStyle = btn.pressed ? "#BBBBBB" : "#FFFFFF";

            ctx.beginPath();
            var w = width;
            var h = height;

            if (direction === "up") {
                ctx.moveTo(w * 0.15, h * 0.7); ctx.lineTo(w * 0.5, h * 0.25); ctx.lineTo(w * 0.85, h * 0.7);
                ctx.lineTo(w * 0.7, h * 0.7); ctx.lineTo(w * 0.5, h * 0.42); ctx.lineTo(w * 0.25, h * 0.7);
            } else if (direction === "down") {
                ctx.moveTo(w * 0.15, h * 0.3); ctx.lineTo(w * 0.5, h * 0.75); ctx.lineTo(w * 0.85, h * 0.3);
                ctx.lineTo(w * 0.7, h * 0.3); ctx.lineTo(w * 0.5, h * 0.58); ctx.lineTo(w * 0.25, h * 0.3);
            } else if (direction === "left") {
                ctx.moveTo(w * 0.7, h * 0.15); ctx.lineTo(w * 0.25, h * 0.5); ctx.lineTo(w * 0.7, h * 0.85);
                ctx.lineTo(w * 0.7, h * 0.7); ctx.lineTo(w * 0.42, h * 0.5); ctx.lineTo(w * 0.7, h * 0.3);
            } else if (direction === "right") {
                ctx.moveTo(w * 0.3, h * 0.15); ctx.lineTo(w * 0.75, h * 0.5); ctx.lineTo(w * 0.3, h * 0.85);
                ctx.lineTo(w * 0.3, h * 0.7); ctx.lineTo(w * 0.58, h * 0.5); ctx.lineTo(w * 0.3, h * 0.3);
            }

            ctx.closePath();
            ctx.fill();
            ctx.stroke();
        }
    }
}