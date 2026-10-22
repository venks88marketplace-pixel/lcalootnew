import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import App 1.0

Item {
    id: root
    implicitWidth: 600
    implicitHeight: 300

    property var processor: appContext.uvCalibrationProcessor

    // Properties for easy dynamic updates
    property string titleText: "UV Irradiance Calibration"
    property string toolId: "TOOL-XXXX"
    property string statusText: "Status: None"
    property real progressValue: processor ? processor.calibrationProgress / 100.0 : 0.0
    property bool calibrationDone: false
    property string buttonText: calibrationDone ? "Done" : "Start"

    // -------------------------------------------------------------------------
    // Top Section
    // -------------------------------------------------------------------------
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
        width: 180
        height: 45
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 20
        anchors.rightMargin: 40

        color: "#8cc63f"
        radius: height / 2
        border.color: "#333333"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: root.statusText
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

        // Background track
        background: Rectangle {
            implicitWidth: progressBar.width
            implicitHeight: progressBar.height
            color: "#dcdcdc"
            radius: height / 2
            border.color: "#333333"
            border.width: 1
        }

        // Fill indicator & centered label
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

    // -------------------------------------------------------------------------
    // Bottom Section: Single Action Button
    // -------------------------------------------------------------------------
    Button {
        id: actionButton
        width: 140
        height: 45
        anchors.top: progressBar.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 20

        enabled: processor && (!processor.isCalibrating || root.calibrationDone)

        onClicked: {
            if (!processor)
                return

            // If complete, button acts as "Done" and navigates home
            if (root.calibrationDone) {
                nav.push("qrc:/qt/qml/App/qml/pages/HeadAssemblySelectionPage.qml")
                return
            }

            // Start flow
            root.calibrationDone = false
            root.statusText = "Status: Starting..."
            root.progressValue = 0.0

            // 1) Connect if needed
            if (!processor.isConnected)
                processor.connectToMeter()

            // 2) Start calibration (mode 0 = Accurate)
            processor.startCalibration(0)
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

    Connections {
        target: processor

        function onConnectedChanged() {
            if (!processor) return
            if (!processor.isCalibrating && !root.calibrationDone) {
                root.statusText = processor.isConnected ? "Status: Connected" : "Status: Disconnected"
            }
        }

        function onCalibrationProgressChanged() {
            if (!processor) return
            root.progressValue = processor.calibrationProgress / 100.0

            if (processor.isCalibrating) {
                root.statusText = "Status: Calibrating"
            } else if (!root.calibrationDone) {
                root.statusText = "Status: " + Math.round(processor.calibrationProgress) + "%"
            }
        }

        function onCalibrationComplete() {
            root.progressValue = 1.0
            root.statusText = "Status: Complete"
            root.calibrationDone = true
        }

        function onCalibrationError(error) {
            root.statusText = "Status: Error"
            root.calibrationDone = false
            console.error("Calibration error:", error)
        }
    }
}