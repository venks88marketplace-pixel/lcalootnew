import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import App 1.0

Item {
    id: root
    property MetaData meta

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Frame {
        //     id: leftPane
        //     SplitView.preferredWidth: 270
        //     //padding: 0
        //     background: Rectangle {
        //         color: "#f0f0f0"
        //         border.color: "#bdbebf"
        //         radius: 5
        //     }

        //     Rectangle {
        //         id: auxControls
        //         visible: true
        //         x: parent.width * 0.01
        //         border.color: "darkblue"
        //         color: "#f0f0f0"
        //         Column {
        //             spacing: 10
        //             leftPadding: 10
        //             topPadding: 10

        //             NearLEDs {}
        //             HardwareTrigger {}
        //             CameraSettings {}
        //             UVControl {}
        //             StepMotor {}
        //         }
        //     }
        // }

        Frame {
            id: rightPane
            SplitView.fillWidth: true
            SplitView.fillHeight: true
            padding: 8

            background: Rectangle {
                color: "#f0f0f0"
                border.color: "#bdbebf"
                radius: 5
            }
            ColumnLayout {
                id: tabBar
                anchors.fill: parent
                spacing: 6

                TabBar {
                    id: bar
                    Layout.fillWidth: true
                    //Layout.minimumHeight: 50
                    TabButton {
                        text: "Camera Offset"
                    }
                    TabButton {
                        text: "Photometric"
                    }
                    TabButton {
                        text: "Stereo Calibration"
                    }
                    TabButton {
                        text: "DMD Calibration"
                    }
                    TabButton {
                        text: "UV Calibration"
                    }
                    TabButton {
                        text: "Logs"
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    currentIndex: bar.currentIndex
                    //CameraOffset {}

                    //meta: root.meta
                    TabCameraOffset {}

                    //meta: root.meta
                    TabPhotometric {}

                    //meta: root.meta
                    TabStereoCalibration {}

                    TabDmdCalibration {}

                    //meta: root.meta
                    TabUVCalibration {}
                    
                    //meta: root.meta
                    LoggerConsole {}
                }
            }
        }
    }
}
