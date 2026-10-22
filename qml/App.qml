pragma ComponentBehavior: Bound
import QtQuick 2.15
import QtQuick.Controls

import App 1.0

ApplicationWindow {
    id: root
    width: 1280
    height: 900
    visible: true
    title: qsTr("Calibration tool")

    StackView {
        id: nav
        anchors.fill: parent

        initialItem: HomePage {}
    }

    Component {
        id: metaPage
        MetaDataPage {
            onContinueToTabs: (session, meta) => {
                appContext.createCalibrationFile(meta.deviceId);
                nav.replace(tabsPage, {
                    session: session,
                    meta: meta
                });
            }
        }
    }

    Component {
        id: tabsPage
        MainTabsPage {}
    }

    Connections {
        target: appContext.authService
        function onSignedIn(session) {
            nav.replace(metaPage, {
                session: session
            });
        }
        function onSignInFailed(reason) {
            console.log("SignIn failed!");
        }
    }
}