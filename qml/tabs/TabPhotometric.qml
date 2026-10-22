import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App 1.0

Item {
    id: root
    anchors.fill: parent
    property var processor: appContext.photometricProcessor
    ColumnLayout {
        anchors.fill: parent

        ImageControl2 {
            id: imctrl
            Layout.preferredWidth: parent.width
            Layout.preferredHeight: 375
            imageControlWidth: 450
            imageControlHeight: 338

            overlayModelLeft: root.processor.leftOverlay
            overlayModelRight: root.processor.rightOverlay
        }
        Rectangle {
            id: messages
            Layout.topMargin: 30
            Layout.leftMargin: 100
            Layout.preferredWidth: 800
            Layout.preferredHeight: 110
            color: "transparent"
            //border.color: "steelblue"
            //border.width: 1

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
        Label {
            id: photoResult
            Layout.alignment: Qt.AlignHCenter
            text: processor.resultStatusText
            //text: processor.passed ? "PASS" : "FAIL"
            color: processor.passed ? "green" : "red"
            font.pixelSize: 20
        }

        RowLayout {
            id: imageAcqButtons
            spacing: 100
            Layout.topMargin: 30
            Layout.leftMargin: 100
            Layout.preferredHeight: 100
            Button {
                id: loadStaticImages
                text: "Load Images"
                Layout.preferredWidth: root.width * 0.14
                Layout.preferredHeight: 60
                onClicked: {
                    root.processor.clearOverlays();
                    root.processor.imageService.getImagePair();
                    console.log("Load Images clicked");
                }
            }
            Button {
                id: acqStartVideo
                Layout.preferredWidth: root.width * 0.14
                checkable: true
                text: checked ? "Stop Video" : "Start Video"
                Layout.preferredHeight: 60
                onClicked: {
                    root.processor.clearOverlays();
                    if (checked === true) {
                        root.processor.imageService.startVideo();
                        console.log("video started");
                    } else {
                        root.processor.imageService.stopVideo();
                        console.log("video stopped");
                    }
                }
            }
            Button {
                id: exitApp
                text: "Exit"
                Layout.preferredWidth: root.width * 0.14
                Layout.preferredHeight: 60
                onClicked: {
                    console.log("Exit button pressed");
                    Qt.quit();
                }
            }
            Item {
                Layout.fillHeight: true   // spacer
            }
        }

        RowLayout {
            id: offsetCalib
            spacing: 100
            Layout.topMargin: 30
            Layout.leftMargin: 100
            Layout.preferredHeight: 100
            Button {
                id: execute
                text: "Execute"
                Layout.preferredWidth: root.width * 0.14
                Layout.preferredHeight: 60
                onClicked: {
                    root.processor.execute("");
                    console.log("Execute clicked");
                }
            }
            Button {
                id: saveRes
                Layout.preferredWidth: root.width * 0.14
                Layout.preferredHeight: 60
                text: "Save"
                onClicked: {
                    console.log("Save clicked");
                }
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
