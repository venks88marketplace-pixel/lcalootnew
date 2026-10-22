import QtQuick 2.5
import QtQuick.Controls.Universal
import QtQuick.Layouts
import ShowPixmap 1.0
import CalibrationTool
import "ui/NearLEDs"
import "ui/CameraSettings"
import "ui/UVControl"
import "ui/StepMotor"

//not stratchable
Item {
    id: root
    width: 1280
    height: 900

    // MotorController {
    //    id: myMotorController
    // }

    //property bool disableStepMotor: myMotorController.disableStepMotor

    //property bool disableStepMotor: false
    property int currentFrameNumberL: 0
    property int currentFrameNumberR: 1

    readonly property int imageWidth: 405 //370
    readonly property int imageHeight: 320 //282

    GridLayout {
        id: camOffset
        columns: 2
        visible: true
        x: 30
        y: 30
        ColumnLayout {
            id: setupForOffset
            Layout.column: 0
            spacing: 0

            NearLEDs {
                id: gbNearLeds
            }

            CameraSettings {
                id: gbCameraSettings
            }

            UVControl {
                id: uvControl
            }

            StepMotor {

            }

            Layout.bottomMargin: 140
        }

        // CameraOffsetFinder {
        //     id:camoffsetFinder
        // }

        ColumnLayout {
            id: runOpsOffset
            Layout.column: 1
            readonly property int w: 850;
            readonly property int h: 790;

            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 20

            Layout.preferredWidth: runOpsOffset.w
            Layout.preferredHeight: runOpsOffset.h
            //color: "lightgrey"
            //border.width: 2
            //border.color: "yellow"
            //radius: 2

            Label { text: qsTr("Status") }
            TextArea { id: info3; readOnly: true; text: "text"; Layout.preferredWidth: runOpsOffset.w; Layout.preferredHeight: 80; Layout.alignment: Qt.AlignCenter }

            RowLayout {
                id: coImages
                spacing: 20
                Layout.alignment: Qt.AlignTop
                Layout.leftMargin: 20
                Column {
                    Text {
                        id: imageLbl
                        text: qsTr("Left image")
                        font.pixelSize: 16
                    }
                    PixmapImage {
                        id:imL
                        width: root.imageWidth
                        height: root.imageHeight
                    }

                }

                Column {
                    Text {
                        id: imageLbR
                        text: qsTr("Right image")
                        font.pixelSize: 16
                    }
                    PixmapImage {
                        id:imR
                        width: root.imageWidth
                        height: root.imageHeight
                    }
                }
                Connections {
                            target: imagefeeder
                            function onImageChanged(imageL, imageR) {
                                imL.setImage( imageL ),
                                imR.setImage( imageR ),
                                console.log("Signal received with value: n/a")
                            }
                        }

            }
            RowLayout {
                id: acqActions
                spacing: 50
                Layout.leftMargin: 20
                Layout.topMargin: 30
                Layout.alignment: Qt.AlignBaseline

                Button {
                    id: loadStaticImages
                    text: "Load Images"
                    Layout.minimumWidth: 150
                    Layout.minimumHeight: 60

                    onClicked: {
                        imagefeeder.getImagePair()
                        console.log("Load Images clicked")
                        //stepMotor.enabled = false
                        //setupForOffset.enabled = !setupForOffset.enabled
                    }
                }
                Button {
                    id: acqStartVideo
                    text: "Start Video"
                    Layout.minimumWidth: 150
                    Layout.minimumHeight: 60
                    onClicked: {
                        imagefeeder.startVideo()
                        console.log("stop button clicked")
                        //stepMotor.enabled = true
                    }
                }

                Button {
                    id: acqStopVideo
                    text: "Stop Video"
                    Layout.minimumWidth: 150
                    Layout.minimumHeight: 60
                    onClicked: {
                        imagefeeder.stopVideo()
                        console.log("stop button clicked")
                        stepMotor.enabled = true

                    }
                }
                Layout.bottomMargin: 30
            }

            RowLayout {
                id: camOffsetOps
                spacing: 50
                Layout.leftMargin: 20
                Layout.alignment: Qt.AlignBaseline

                Button {
                    id: coCalStart
                    text: "Calibrate"
                    Layout.minimumWidth: 150
                    Layout.minimumHeight: 60
                    onClicked: {
                        console.log("calibrate button clicked")
                        //stepMotor.enabled = false
                        setupForOffset.enabled = !setupForOffset.enabled
                        //var result = ctImageProcessor.computeOffets()
                    }
                }
                Button {
                    id: coCalStop
                    text: "Stop calibration"
                    Layout.minimumWidth: 150
                    Layout.minimumHeight: 60
                    onClicked: {
                        console.log("stop button clicked")
                        stepMotor.enabled = true
                    }
                }

                Button { id: coCalSave; text: "SAVE"; Layout.minimumWidth: 150; Layout.minimumHeight: 60; Layout.leftMargin: 100}

                Button { id: coCalEnd; text: "END"; Layout.minimumWidth: 150; Layout.minimumHeight: 60; onClicked: Qt.quit() }

                Layout.bottomMargin: 160
            }
        }
    }
}



