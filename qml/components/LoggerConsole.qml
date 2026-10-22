import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import App 1.0

Rectangle {
    id: loggerConsole
    color: "#222"
    opacity: 0.94
    radius: 8

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Logs"
                color: "#ddd"
                font.bold: true
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: "Clear"
                onClicked: logModel.clear()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: logModel
            delegate: Text {
                text: level + ": " + msg
                color: level === "error" ? "#f55" : (level === "warn" ? "#fb5" : "#9cf")
                font.family: "Monospace"
                wrapMode: Text.Wrap
            }
        }
    }

    ListModel {
        id: logModel
    }

    Connections {
        target: appContext.logger
        function onMessageLogged(level, msg) {
            logModel.append({
                level,
                msg
            });
            list.positionViewAtEnd();
        }
    }
}
