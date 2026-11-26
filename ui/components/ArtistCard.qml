import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: artistCard

    property int w: 210
    property int h: 210
    property int imageRadius: 105

    required property var artistId
    required property string name
    required property int trackCount
    required property int playCount
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
            id: coverContainer
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: artistCard.width - 20
            Layout.preferredHeight: Layout.preferredWidth
            radius: artistCard.imageRadius
            color: "#27272a"

            Image {
                id: coverImage
                anchors.fill: parent
                source: artistCard.coverUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: artistCard.coverUrl !== ""

                layer.enabled: true
                layer.effect: MultiEffect {
                    maskEnabled: true
                    maskSource: coverMask
                }
            }

            Rectangle {
                id: coverMask
                anchors.fill: parent
                radius: coverContainer.radius
                color: "white"
                antialiasing: true
                visible: false
                layer.enabled: true
            }
        }

        Text {
            Layout.fillWidth: true
            text: artistCard.name
            font.pixelSize: 14
            font.bold: true
            color: "#f4f4f6"
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: artistCard.trackCount + (artistCard.trackCount !== 1 ? " songs" : " song")
            font.pixelSize: 12
            color: "#8e8e8e"
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: artistCard.playCount + " reprod."
            font.pixelSize: 12
            color: "#8e8e8e"
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            visible: playCount !== -1
        }
    }

    MouseArea {
        id: cardMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            appController.openArtistSubView(artistCard.artistId)
            root.subViewCircular = false
            root.subViewType = "artist"
            root.inSubView = true
        }
    }
}