import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: playlistCard

    property int w: 156
    property int h: 210
    property int imageRadius: 8

    required property var playlistId
    required property string name
    required property int trackCount
    required property int durationSecs
    required property string coverUrl

    signal openRequested(var selectedPlaylistId)

    width: w
    height: h
    radius: 10
    color: cardMouse.containsMouse ? "#232329" : "#18181c"

    function formatDuration(totalSecs) {
        if (!totalSecs || totalSecs <= 0) return "0 min"

        const hours = Math.floor(totalSecs / 3600)
        const mins = Math.floor((totalSecs % 3600) / 60)

        if (hours > 0) {
            return mins > 0 ? (hours + " h " + mins + " min") : (hours + " h")
        }
        return Math.max(1, mins) + " min"
    }

    onOpenRequested: {
        playlistsView.playlistSelected(selectedPlaylistId)
    }

    ColumnLayout {
        id: contentBox
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        Rectangle {
            id: coverContainer
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: playlistCard.width - 20
            Layout.preferredHeight: Layout.preferredWidth
            radius: playlistCard.imageRadius
            color: "#27272a"

            Image {
                id: coverImage
                anchors.fill: parent
                source: playlistCard.coverUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: playlistCard.coverUrl !== ""

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
            text: playlistCard.name
            font.pixelSize: 14
            font.bold: true
            color: "#f4f4f6"
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: playlistCard.trackCount + (trackCount === 1 ? " song" : " songs")
            font.pixelSize: 12
            color: "#8e8e8e"
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }
        
        Text {
            Layout.fillWidth: true
            text: formatDuration(durationSecs)
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
            playlistCard.openRequested(playlistCard.playlistId)
        }
    }
}