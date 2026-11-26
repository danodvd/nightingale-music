import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: albumCard

    property int w: 156
    property int h: 210
    property int imageRadius: 8

    required property var albumId
    required property string name
    required property int trackCount
    required property string artists
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
            Layout.preferredWidth: albumCard.width - 20
            Layout.preferredHeight: Layout.preferredWidth
            radius: albumCard.imageRadius
            color: "#27272a"
            clip: true

            Image {
                anchors.fill: parent
                source: albumCard.coverUrl
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                visible: albumCard.coverUrl !== ""
            }
        }

        Text {
            Layout.fillWidth: true
            text: albumCard.name
            font.pixelSize: 14
            font.bold: true
            color: "#f4f4f6"
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: albumCard.artists
            font.pixelSize: 14
            color: "#8e8e8e"
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: albumCard.trackCount  + (albumCard.trackCount !== 1 ? " songs" : " song")
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
        onClicked: {
            appController.openAlbumSubView(albumId)
            root.subViewCircular = false
            root.subViewType = "album"
            root.inSubView = true 
        }
    }
}