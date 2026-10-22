import QtQuick
import QtQuick.Shapes

import App 1.0

Item {
    id: root
    required property var model
    property real imageWidth: 0
    property real imageHeight: 0

    property var colorFilter: function (c) {
        return c;
    }

    readonly property real sx: root.imageWidth > 0 ? root.width / root.imageWidth : 1
    readonly property real sy: root.imageHeight > 0 ? root.height / root.imageHeight : 1
    readonly property real scaleFactor: Math.min(sx, sy)

    readonly property real displayedImageWidth: imageWidth * scaleFactor
    readonly property real displayedImageHeight: imageHeight * scaleFactor

    readonly property real offsetX: (root.width - displayedImageWidth) / 2
    readonly property real offsetY: (root.height - displayedImageHeight) / 2

    function mapX(x) {
        return  x * scaleFactor;
    }
    function mapY(y) {
        return y * scaleFactor;
    }
    function mapW(w) {
        return w * scaleFactor;
    }
    function mapH(h) {
        return h * scaleFactor;
    }

    onImageWidthChanged: console.log("Overlay X scale: , imageWidth: ", root.sx, imageWidth)
    onImageHeightChanged: console.log("Overlay Y scale: , imageHeight", root.sy, imageHeight)

    Component.onCompleted: {
        if (model.shapeType === ShapeModel.Polyline) {
            console.log("POLYLINE:",
                        "points =", model.points.length,
                        "first =", model.points.length > 0 ? model.points[0] : "none",
                        "scale =", root.scaleFactor,
                        "offset =", root.offsetX, root.offsetY)
        }
        console.log(`------------User Info: width: ${width}, imageWidth: ${imageWidth}, sx: ${sx}`);
        console.log(`------------User Info: width: ${height}, imageHeight: ${imageHeight}, sy: ${sy}`);
        console.log(`------------User Info: offsetX: ${offsetX}`);
        console.log(`------------User Info: offsetY: ${offsetY}`);
        console.log(`------------User Info: displayedImageWidth: ${displayedImageWidth}`);
        console.log(`------------User Info: displayedImageHeight: ${displayedImageHeight}`);
        console.log("OverlayCanvas model =", model);
    }
    onWidthChanged: console.log("OverlayCanvas width:", width)
    onHeightChanged: console.log("OverlayCanvas height:", height)

    Repeater {
        model: root.model

        Item {
            width: root.width
            height: root.height
            z: layer
            Shape {
                visible: model.shapeType === ShapeModel.Circle
                ShapePath {
                    strokeWidth: 2
                    strokeColor: root.colorFilter(model.color)
                    fillColor: "transparent"
                    startX: root.mapX(model.p1.x) - root.mapX(model.p2.x)
                    startY: mapY(model.p1.y)
                    PathArc {
                        x: root.mapX(model.p1.x) + root.mapX(model.p2.x)
                        y: root.mapY(model.p1.y)
                        radiusX: root.mapX(model.p2.x)
                        radiusY: root.mapY(model.p2.y)
                        useLargeArc: false
                    }
                    PathArc {
                        x: root.mapX(model.p1.x) - root.mapX(model.p2.x)
                        y: root.mapY(model.p1.y)
                        radiusX: root.mapX(model.p2.x)
                        radiusY: root.mapY(model.p2.y)
                        useLargeArc: false
                    }
                }
            }

            Shape {
                visible: model.shapeType === ShapeModel.Rect
                ShapePath {
                    strokeWidth: 2
                    strokeColor: root.colorFilter(model.color)
                    fillColor: "transparent"
                    startX: root.mapX(model.p1.x)
                    startY: root.mapY(model.p1.y)

                    PathLine {
                        x: root.mapX(model.p1.x) + root.mapX(model.p2.x)
                        y: root.mapY(model.p1.y)
                    }
                    PathLine {
                        x: root.mapX(model.p1.x) + root.mapX(model.p2.x)
                        y: root.mapY(model.p1.y) + root.mapY(model.p2.y)
                    }
                    PathLine {
                        x: root.mapX(model.p1.x)
                        y: root.mapY(model.p1.y) + root.mapY(p2.y)
                    }
                    PathLine {
                        x: root.mapX(model.p1.x)
                        y: root.mapY(model.p1.y)
                    }
                }
            }

            Shape {
                visible: model.shapeType === ShapeModel.Line
                ShapePath {
                    strokeWidth: 2
                    strokeColor: colorFilter(model.color)
                    fillColor: "transparent"
                    startX: root.mapX(model.p1.x)
                    startY: root.mapY(model.p1.y)

                    PathLine {
                        x: root.mapX(model.p2.x)
                        y: root.mapY(model.p2.y)
                    }
                }
            }

            Shape {
                visible: model.shapeType === ShapeModel.Polyline
                transform: [
                    Translate {
                         x: root.offsetX - 60 // needs fix
                         y: root.offsetY
                    },
                    Scale {
                        origin.x: 0
                        origin.y: 0
                        xScale: root.sx
                        yScale: root.sy
                    }
                ]

                ShapePath {
                    strokeColor: root.colorFilter(model.color)
                    strokeWidth: 1.0 / root.scaleFactor
                    fillColor: "transparent"
                    startX: model.points.length > 0 ? model.points[0].x : 0
                    startY: model.points.length > 0 ? model.points[0].y : 0

                    PathPolyline {
                        path: model.points
                    }
                }
            }

            Text {
                visible: model.shapeType === ShapeModel.Label
                x: root.mapX(model.p1.x)
                y: root.mapY(model.p1.y)
                text: model.text
                color: root.colorFilter(model.color)
                font.pixelSize: 16
                font.bold: true
                z: 9999
            }
            Component.onCompleted: {
                if (shapeType === ShapeModel.Label)
                    console.log("LABEL DELEGATE created:", p1, "text role =", model.text, "color =", color);
            }
        }
    }
}
