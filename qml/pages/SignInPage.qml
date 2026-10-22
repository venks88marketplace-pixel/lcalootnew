import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import App 1.0

Item {
    id: root

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 10

        Label {
            text: "Sign In"
            font.pixelSize: 22
            font.bold: true
        }

        TextField {
            id: user
            placeholderText: "Username"
            Layout.preferredWidth: 320
        }
        TextField {
            id: pass
            placeholderText: "Password"
            echoMode: TextInput.Password
            Layout.preferredWidth: 320
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "Sign In"
                onClicked: {
                    appContext.authService.signIn(user.text, pass.text);
                    console.log("sign in clicked");
                }
            }
            Button {
                text: "Demo User"
                onClicked: {
                    user.text = "anyone";
                    pass.text = "x";
                    console.log("sign in clicked");
                }
            }
        }
        Label {
            text: "Any non-empty username succeeds (demo)."
            opacity: 0.75
        }
    }
}
