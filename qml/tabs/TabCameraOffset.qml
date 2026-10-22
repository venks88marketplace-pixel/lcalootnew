/*
import QtQuick 2.15
import QtQuick.Controls.Universal
import CalibrationTool

//not stratchable
Item {
    id: root
    width: 990
    height: 900

    readonly property int imageWidth: 370 //370
    readonly property int imageHeight: 266 //282

    // lwidth: root.imageWidth
    // lheight: root.imageHeight

    // //property alias lheight: leftImage.height

    // property alias rwidth: rightImage.width
    // property alias rheight: rightImage.height


    Rectangle {
        id: auxControls
        visible: true
        x: parent.width * 0.01
        y: parent.height * 0.02

        border.color: "red"

        height: parent.height * 0.90
        width: parent.width * 0.242

        // DeviceControls {
        //     id: deviceControls
        // }
    }

    ImageControl {
        id: imageControl
        anchors {
            left: auxControls.right
            leftMargin: 18
            top: parent.top
            topMargin: 20
        }
        imWidthL: root.imageWidth
        imHeightL: root.imageHeight

        imWidthR: root.imageWidth
        imHeightR: root.imageHeight
    }

    Rectangle {
        id: procCtrl
        visible: true
        border.color: "blue"
        height: parent.height * 0.12
        width: parent.width * 0.72
        anchors {
            left: auxControls.right
            leftMargin: 18
            top: imageControl.bottom
            topMargin: 20
        }
        Row {
            id: procCtrlBtns
            spacing: 50
            leftPadding: 150
            topPadding: 30

            property int buttonWidth: imageControl.width * 0.14
            Button {
                id: processImage
                text: "Process Image"
                width: procCtrlBtns.buttonWidth
                height: 60
                onClicked: {
                    console.log("Process Image clicked");
                    offsetCalib.ComputeCameraOffsets();
                    //stepMotor.enabled = false
                    //setupForOffset.enabled = !setupForOffset.enabled
                }
            }
            Button {
                id: saveResults
                text: "Save Results"
                width: procCtrlBtns.buttonWidth
                height: 60
                onClicked: {
                    console.log("Save Results button clicked");
                    //stepMotor.enabled = true
                }
            }
            Button {
                id: exitTask
                text: "Exit"
                width: procCtrlBtns.buttonWidth
                height: 60
                onClicked: {
                    console.log("Exit button clicked");
                    //stepMotor.enabled = true
                }
            }
        }
    }

    Rectangle {
        id: procInfo
        visible: true
        border.color: "blue"
        height: parent.height * 0.15
        width: parent.width * 0.72
        anchors {
            left: auxControls.right
            leftMargin: 18
            top: procCtrl.bottom
            topMargin: 20
            bottom: auxControls.bottom
        }
    }
}
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import App 1.0

Item {
    id: root
    anchors.fill: parent
    property var processor: appContext.cameraOffsetProcessor
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
        if (!visible && processor) {
            processor.clearOverlays();
        }
    }
    Item {
        Layout.fillHeight: true   // spacer
    }
}
