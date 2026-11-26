import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property int w
    required property int h
    required property int r
    required property string icon
    required property var callback
    property string textColor: "#f4f4f6"
    property int fontSize: 28

    width: w
    height: h

    Layout.preferredWidth: w
    Layout.preferredHeight: h

    color: "transparent"
    radius: r

    function command() {
        if (typeof callback === "function")
            callback()
    }

    Text {
        anchors.centerIn: parent
        text: root.icon
        font.pixelSize: fontSize
        color: mouseArea.containsMouse ? "#a5a5f4" : textColor
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.command()
    }
}