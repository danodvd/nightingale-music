import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../components/"

ColumnLayout {
    spacing: 12

    TextField {
        placeholderText: "Search for Albums..."
        Layout.fillWidth: true
        onTextChanged: appController.searchAlbums(text)
    }

    GridView {
    id: albumGrid
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true

        readonly property int cardWidth: 164
        readonly property int cardHeight: 224
        readonly property int gap: 14

        cellWidth: cardWidth + gap
        cellHeight: cardHeight + gap
        model: appController.albumsModel

        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            width: albumGrid.cellWidth
            height: albumGrid.cellHeight

            required property var albumId
            required property string name
            required property string artists
            required property string coverUrl
            required property int trackCount

            AlbumCard {
                anchors.centerIn: parent
                w: albumGrid.cardWidth
                h: albumGrid.cardHeight
                albumId: parent.albumId
                name: parent.name
                trackCount: parent.trackCount
                artists: parent.artists
                coverUrl: parent.coverUrl
            }
        }
    }
}