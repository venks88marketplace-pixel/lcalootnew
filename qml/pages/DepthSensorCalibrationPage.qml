import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    implicitWidth: 600
    implicitHeight: 300

    property string titleText: "Depth Sensor Calibration"
    property string toolId: "TOOL-XXXX"
    property real progressValue: appContext.depthSensorService.progress
    property bool isDone: progressValue >= 1.0
    property string buttonText: isDone ? "Done"
                                       : appContext.depthSensorService.busy ? "Running"
                                                                            : "Start"

    Column {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 15
        spacing: 4

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.titleText
            font.pixelSize: 14
            font.bold: true
            color: "#000000"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.toolId
            font.pixelSize: 13
            color: "#000000"
        }
    }

    Rectangle {
        id: statusBadge
        width: 160
        height: 45
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 20
        anchors.rightMargin: 40

        color: appContext.depthSensorService.statusText.indexOf("Pass") !== -1 ? "#8cc63f"
             : appContext.depthSensorService.statusText.indexOf("Fail") !== -1 ? "#e57373"
             : "#fbb03b"
        radius: height / 2
        border.color: "#333333"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: root.isDone ? "Complete" : (appContext.depthSensorService.busy ? "Running" : "Ready")
            font.pixelSize: 12
            color: "#000000"
        }
    }

    ProgressBar {
        id: progressBar
        width: parent.width * 0.8
        height: 44
        anchors.centerIn: parent
        from: 0.0
        to: 1.0
        value: root.progressValue

        background: Rectangle {
            implicitWidth: progressBar.width
            implicitHeight: progressBar.height
            color: "#dcdcdc"
            radius: height / 2
            border.color: "#333333"
            border.width: 1
        }

        contentItem: Item {
            implicitWidth: progressBar.width
            implicitHeight: progressBar.height

            Rectangle {
                width: progressBar.visualPosition * parent.width
                height: parent.height
                color: "#fbb03b"
                radius: parent.height / 2
                border.color: "#333333"
                border.width: 1
            }

            Text {
                anchors.centerIn: parent
                text: "Progress (" + Math.round(progressBar.position * 100) + "%)"
                font.pixelSize: 13
                color: "#000000"
            }
        }
    }

    // Show only while busy; hidden before start and after done/fail
    Text {
        id: liveStatusText
        anchors.top: progressBar.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 12
        visible: appContext.depthSensorService.busy
        text: appContext.depthSensorService.statusText
        font.pixelSize: 13
        color: "#000000"
        horizontalAlignment: Text.AlignHCenter
    }

    Button {
        id: actionButton
        width: 140
        height: 45
        anchors.top: liveStatusText.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 14

        enabled: root.isDone || !appContext.depthSensorService.busy

        onClicked: {
            if (root.isDone) {
                if (typeof nav !== "undefined")
                    nav.pop()
            } else {
                appContext.depthSensorService.startCalibrationRun()
            }
        }

        contentItem: Text {
            text: root.buttonText
            font.pixelSize: 13
            color: "#000000"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            color: actionButton.down ? "#7ab332" : "#8cc63f"
            radius: height / 2
            border.color: "#333333"
            border.width: 1
        }
    }
}