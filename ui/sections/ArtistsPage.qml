import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../components/"

ColumnLayout {
    spacing: 12

    TextField {
        placeholderText: "Search for artists..."
        Layout.fillWidth: true
        onTextChanged: appController.searchArtists(text)
    }

    GridView {
    id: artistGrid
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true

        readonly property int cardWidth: 164
        readonly property int cardHeight: 224
        readonly property int gap: 14

        cellWidth: cardWidth + gap
        cellHeight: cardHeight + gap
        model: appController.artistsModel

        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            width: artistGrid.cellWidth
            height: artistGrid.cellHeight

            required property var artistId
            required property string name
            required property int trackCount
            property int playCount: -1
            required property string coverUrl

            ArtistCard {
                anchors.centerIn: parent
                w: artistGrid.cardWidth
                h: artistGrid.cardHeight
                artistId: parent.artistId
                name: parent.name
                trackCount: parent.trackCount
                playCount: parent.playCount
                coverUrl: parent.coverUrl
            }
        }
    }
}