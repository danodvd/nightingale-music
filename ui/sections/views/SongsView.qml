import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import "../../components/"

ColumnLayout {
    spacing: 12

    TextField {
        placeholderText: "Search tracks..."
        Layout.fillWidth: true
        onTextChanged: appController.searchTracks(text)
    }

    GridView {
        id: libraryGrid
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true

        readonly property int cardWidth: 164
        readonly property int cardHeight: 224
        readonly property int gap: 14

        cellWidth: cardWidth + gap
        cellHeight: cardHeight + gap
        model: appController.tracksModel

        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            width: libraryGrid.cellWidth
            height: libraryGrid.cellHeight

            required property var trackId
            required property string title
            required property string artist
            required property string coverUrl

            TrackCard {
                anchors.centerIn: parent
                w: libraryGrid.cardWidth
                h: libraryGrid.cardHeight
                trackId: parent.trackId
                title: parent.title
                artist: parent.artist
                coverUrl: parent.coverUrl
            }
        }
    }
}