import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: trackCard

    property int w: 156
    property int h: 210
    property int imageRadius: 8

    required property var trackId
    required property string title
    required property string artist
    required property string coverUrl

    width: w
    height: h
    radius: 10
    color: cardMouse.containsMouse ? "#232329" : "#18181c"

    ColumnLayout {
        id: contentBox
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: trackCard.width - 20
            Layout.preferredHeight: Layout.preferredWidth
            radius: trackCard.imageRadius
            color: "#27272a"
            clip: true

            Image {
                anchors.fill: parent
                source: trackCard.coverUrl
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                visible: trackCard.coverUrl !== ""
            }
        }

        Text {
            Layout.fillWidth: true
            text: trackCard.title
            font.pixelSize: 14
            font.bold: true
            color: "#f4f4f6"
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: trackCard.artist
            font.pixelSize: 12
            color: "#8e8e8e"
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }
    }

    MouseArea {
        id: cardMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: appController.playTrackById(trackCard.trackId)
    }
}