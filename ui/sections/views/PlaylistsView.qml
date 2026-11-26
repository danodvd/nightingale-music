import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../../components/"



ColumnLayout {
    id: playlistsView
    
    spacing: 12

    signal playlistSelected(var playlistId)

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
        model: appController.playlistsModel

        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            width: libraryGrid.cellWidth
            height: libraryGrid.cellHeight

            required property var playlistId
            required property string name
            required property int trackCount
            required property string coverUrl
            required property int durationSecs

            PlaylistCard {
                anchors.centerIn: parent
                w: libraryGrid.cardWidth
                h: libraryGrid.cardHeight
                playlistId: parent.playlistId
                name: parent.name
                trackCount: parent.trackCount
                durationSecs: parent.durationSecs
                coverUrl: parent.coverUrl
            }
        }
    }
}