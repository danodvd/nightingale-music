import QtQuick
import QtQuick.Layouts

Rectangle {
    id: navButton
    implicitHeight: content.implicitHeight
    Layout.fillWidth: true

    property color fontColor: "#ffffff"
    property string icon: ""
    property string name: ""

    property var command

    ColumnLayout {
        id: content
        anchors.centerIn: parent
        spacing: 3
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: navButton.icon
            font.pixelSize: 24
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: navButton.name
            color: "#f4f4f6"
            font.pixelSize: 14
        }
    }
    MouseArea {
        anchors.fill: parent
        onClicked: command
    }
}