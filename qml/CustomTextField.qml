import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Layouts

TextField {
    id: root

    property color borderColor: "black"
    property int borderWidth: 1
    property string placeholder: ""
    property color textColor: "black"

    implicitWidth: 65
    implicitHeight: 24

    placeholderText: placeholder
    font.pixelSize: 12
    padding: 0
    color: textColor
    horizontalAlignment: Qt.AlignHCenter
    verticalAlignment: Qt.AlignVCenter
    //width: root.boxWidth
    background: Rectangle {
        border.width: root.borderWidth
    }
}
