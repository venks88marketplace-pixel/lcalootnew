import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App 1.0

Item {
    id: root
    anchors.fill: parent
    property var processor: appContext.uvCalibrationProcessor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // ===== Status Section =====
        Rectangle {
            Layout.fillWidth: true
            height: 80
            color: "#aaaaaa"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4

                Text {
                    text: "UV Calibration Tool"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#ffffff"
                }

                RowLayout {
                    Text {
                        text: "Ophir Meter:"
                        color: "#aaaaaa"
                        font.pixelSize: 12
                    }
                    Text {
                        id: ophirStatus
                        text: processor ? (processor.isConnected ? "✓ Connected" : "✗ Disconnected") : "Unknown"
                        color: processor && processor.isConnected ? "#00aa00" : "#aa0000"
                        font.bold: true
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Server:"
                        color: "#aaaaaa"
                        font.pixelSize: 12
                    }
                    Text {
                        id: serverStatus
                        text: processor ? (processor.serverConnected ? "✓ Connected" : "✗ Disconnected") : "Unknown"
                        color: processor && processor.serverConnected ? "#00aa00" : "#aa0000"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }
            }
        }

        // ===== Connection Buttons =====
        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: "#aaaaaa"
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                Button {
                    text: "Connect Ophir"
                    Layout.preferredWidth: 120
                    enabled: processor && !processor.isConnected
                    onClicked: {
                        if (processor) processor.connectToMeter()
                    }
                }

                Button {
                    text: "Disconnect Ophir"
                    Layout.preferredWidth: 140
                    enabled: processor && processor.isConnected
                    onClicked: {
                        if (processor) processor.disconnectFromMeter()
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        // ===== Current Reading Display =====
        Rectangle {
            Layout.fillWidth: true
            height: 120
            color: "#aaaaaa"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                Text {
                    text: "Current Irradiance Reading (5mm sensor)"
                    font.pixelSize: 14
                    font.bold: true
                    color: "#ffffff"
                }

                Text {
                    id: readingLabel
                    text: processor ? processor.currentReading.toFixed(3) + " mW/cm²" : "0.000 mW/cm²"
                    font.pixelSize: 36
                    font.bold: true
                    color: "#00aa00"
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }

                ProgressBar {
                    Layout.fillWidth: true
                    value: processor ? Math.min(processor.currentReading / 60.0, 1.0) : 0
                    height: 10
                }
            }
        }

        // ===== Calibration Mode Selection =====
        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: "#aaaaaa"
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                Text {
                    text: "Calibration Mode:"
                    color: "#ffffff"
                    font.pixelSize: 12
                    font.bold: true
                }

                ComboBox {
                    id: calibrationModeCombo
                    Layout.preferredWidth: 200
                    model: ["Accurate (±1.5% tolerance)", "Demo (all DAC values)"]
                    enabled: processor && processor.isConnected && !processor.isCalibrating
                }

                Item { Layout.fillWidth: true }
            }
        }

        // ===== Calibration Control =====
        Rectangle {
            Layout.fillWidth: true
            height: 120
            color: "#aaaaaa"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Text {
                        text: "Calibration"
                        font.pixelSize: 14
                        font.bold: true
                        color: "#ffffff"
                    }

                    Text {
                        id: calibStatus
                        text: processor ? (processor.isCalibrating ? "Running..." : "Ready") : "Unknown"
                        color: processor && processor.isCalibrating ? "#ffaa00" : "#00aa00"
                        font.bold: true
                        font.pixelSize: 12
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: "PD1: 10"
                        color: "#ffffff"
                        font.pixelSize: 11
                    }

                    Text {
                        text: "Stabilization: 3s"
                        color: "#ffffff"
                        font.pixelSize: 11
                    }
                }

                RowLayout {
                    spacing: 12

                    Button {
                        text: "Start Calibration"
                        Layout.preferredWidth: 150
                        enabled: processor && processor.isConnected && !processor.isCalibrating
                        onClicked: {
                            if (processor) {
                                // Pass 0 for Accurate mode, 1 for Demo mode
                                processor.startCalibration(calibrationModeCombo.currentIndex)
                            }
                        }
                    }

                    Button {
                        text: "Stop Calibration"
                        Layout.preferredWidth: 140
                        enabled: processor && processor.isCalibrating
                        onClicked: {
                            if (processor) processor.stopCalibration()
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                ProgressBar {
                    Layout.fillWidth: true
                    value: processor ? processor.calibrationProgress / 100.0 : 0
                }

                Text {
                    text: (processor ? processor.calibrationProgress : 0) + "% Complete"
                    color: "#aaaaaa"
                    font.pixelSize: 11
                }
            }
        }

        // ===== Log Output =====
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#aaaaaa"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "Log Output"
                    font.pixelSize: 14
                    font.bold: true
                    color: "#ffffff"
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextArea {
                        id: logOutput
                        readOnly: true
                        text: "UV Calibration Tool initialized\n"
                        color: "#00aa00"
                        font.family: "Courier"
                        font.pixelSize: 10
                        background: Rectangle {
                            color: "#222222"
                            border.color: "#666666"
                            border.width: 1
                        }
                    }
                }
            }
        }
    }

    // Update log when messages come in
    Connections {
        target: processor
        function onLogMessage(message) {
            logOutput.append(message)
            logOutput.cursorPosition = logOutput.length - 1
        }
    }

    // Update UI when properties change
    Connections {
        target: processor
        function onReadingUpdated() {
            readingLabel.text = processor.currentReading.toFixed(3) + " mW/cm²"
        }
        function onConnectedChanged() {
            ophirStatus.text = processor.isConnected ? "✓ Connected" : "✗ Disconnected"
            ophirStatus.color = processor.isConnected ? "#00aa00" : "#aa0000"
        }
        function onCalibrationComplete() {
            calibStatus.text = "Complete!"
            calibStatus.color = "#00aa00"
            // Reset after 2 seconds
            resetCalibrationStatusTimer.start()
        }
        function onCalibrationError(error) {
            logOutput.append("ERROR: " + error)
        }
    }

    // Timer to reset calibration status
    Timer {
        id: resetCalibrationStatusTimer
        interval: 2000
        onTriggered: {
            if (processor && !processor.isCalibrating) {
                calibStatus.text = "Ready"
                calibStatus.color = "#00aa00"
            }
        }
    }

    onVisibleChanged: {
        if (!visible && processor) {
            processor.stopCalibration()
        }
    }
}