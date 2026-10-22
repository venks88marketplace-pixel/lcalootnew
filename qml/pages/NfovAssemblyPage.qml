import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"

Page {
    id: root

    readonly property bool isReallyVisible: visible && parent && parent.visible

    property alias leftMtfText: leftPanel.mtfText
    property alias rightMtfText: rightPanel.mtfText

    signal leftPanRequested(string direction)  // "up", "down", "left", "right"
    signal rightPanRequested(string direction) // "up", "down", "left", "right"

    background: Rectangle {
        color: "#FFFFFF"
    }

    // Main Layout
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 12

        // Header Section
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 2

            Text {
                text: "NFOV Assembly Calibration"
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

        // Camera Panels Row
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 20

            CameraControlPanel {
                id: leftPanel
                Layout.fillWidth: true
                Layout.fillHeight: true        
                placeholderText: "NFOV Left"
                mtfText: "MTF: 60%"
                imageModel: appContext.cameraOffsetProcessor.leftOverlay        
                onPanRequested: (direction) => root.leftPanRequested(direction)
            }

            CameraControlPanel {
                id: rightPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                placeholderText: "NFOV Right"
                mtfText: "MTF: 60%"
                imageModel: appContext.cameraOffsetProcessor.rightOverlay        
                onPanRequested: (direction) => root.rightPanRequested(direction)
            }    
        }

        // Action Button
        Button {
            id: doneBtn
            text: "Done"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Layout.preferredHeight: 40

            background: Rectangle {
                color: doneBtn.down ? "#7CB342" : "#8BC34A"
                border.color: "#333333"
                border.width: 1
                radius: 20
            }

            contentItem: Text {
                text: doneBtn.text
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