import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import App 1.0

Item {
    id: root
    anchors.fill: parent
    property var processor: appContext.dmdCalibrationProcessor
    ColumnLayout {
        anchors.fill: parent
        ImageControl2 {
            id: imctrl
             Layout.leftMargin: 70
            Layout.preferredWidth: parent.width
            Layout.preferredHeight: 375
            imageControlWidth: 450
            imageControlHeight: 338

            overlayModelLeft: root.processor.leftOverlay
            overlayModelRight: root.processor.rightOverlay
        }
        Rectangle {
            id: messages
            Layout.topMargin: 5
            Layout.leftMargin: 85
            Layout.preferredWidth: 930
            Layout.preferredHeight: 50
            color: "transparent"

            ListView {
                id: messageView
                anchors.fill: parent
                anchors.margins: 4
                model: root.processor.uiMessage
                clip: true
                onCountChanged: positionViewAtEnd()

                delegate: Flow {
                    width: messageView.width
                    spacing: 10
                    required property var messageParts
                    Repeater {
                        model: messageParts
                        Text {
                            text: modelData.text
                            color: modelData.color
                            wrapMode: Text.NoWrap
                            font.pixelSize: 14
                        }
                    }
                }
            }
        }

        RowLayout {
            id: imageAcqButtons
            spacing: 100
            Layout.topMargin: 30
            Layout.leftMargin: 150
            Layout.preferredHeight: 100

            Button {
                id: calibrate
                text: "Calibrate"
                Layout.preferredWidth: root.width * 0.2
                Layout.preferredHeight: 70
                onClicked: {
                    root.processor.execute("");
                    console.log("Calibrate clicked");
                }
            }
            Button {
                id: saveRes
                Layout.preferredWidth: root.width * 0.2
                Layout.preferredHeight: 70
                text: "Save"
                onClicked: {
                    console.log("Save clicked");
                }
            }
            Button {
                id: exitApp
                text: "Exit"
                Layout.preferredWidth: root.width * 0.2
                Layout.preferredHeight: 70
                onClicked: {
                    console.log("Exit button pressed");
                    Qt.quit();
                }
            }
        }

        ColumnLayout{
            spacing: 6
            Label {
                 id: statusLabel
                 Layout.leftMargin: 130
                 Layout.preferredWidth: root.width * 0.8
                 text: root.processor.statusMessage
                 elide: Text.ElideRight
                 wrapMode: Text.NoWrap
                 font.pixelSize: 14
             }
            RowLayout {
                spacing: 10
                ProgressBar {
                    Layout.leftMargin: 130
                    Layout.preferredWidth: root.width * 0.8
                    height: 40
                    from: 0.0
                    to: 1.0
                    value: processor.progress
                }
                Text {
                    id: percentage
                    text: Math.round(processor.progress * 100) + "%"
                }
            }
            Item {
                Layout.fillHeight: true   // spacer
            }
        }
    }
    onVisibleChanged: {
        if (!visible && processor)
            processor.clearOverlays();
    }
    Item {
        Layout.fillHeight: true   // spacer
    }
}
