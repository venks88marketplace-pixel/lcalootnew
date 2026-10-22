import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App 1.0

Item {
    id: root
    property Session session

    signal continueToTabs(Session session, MetaData meta)

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 10

        Label {
            text: "Session Meta Data"
            font.pixelSize: 20
            font.bold: true
        }
        Label {
            text: "Signed in as: " + (session ? session.userId : "<null>")
            opacity: 0.85
        }

        TextField {
            id: project
            placeholderText: "Project ID"
            Layout.preferredWidth: 420
            text: "Job ID"
        }
        TextField {
            id: device
            placeholderText: "System serial number"
            Layout.preferredWidth: 420
            text: "System S/N"
        }
        TextArea {
            id: notes
            placeholderText: "Notes"
            Layout.preferredWidth: 420
            Layout.preferredHeight: 120
            //text: "demo run"
        }

        RowLayout {
            Button {
                text: "Continue"
                onClicked: {
                    const meta = Qt.createQmlObject('import App; MetaData {}', root);
                    meta.projectId = project.text;
                    meta.deviceId = device.text;
                    meta.notes = notes.text;
                    continueToTabs(session, meta);
                    console.log("MetaDatePage continue clicked");
                }
            }
        }
    }
}
