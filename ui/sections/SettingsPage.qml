import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    spacing: 12

    TextField {
        placeholderText: "Buscar canciones..."
        Layout.fillWidth: true
        onTextChanged: appController.searchTracks(text)
    }

    ListView {
        id: tracksView
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: appController.tracksModel
        spacing: 2

        delegate: ItemDelegate {
            required property string title
            required property string artist
            required property var trackId
            width: tracksView.width
            text: title + " — " + artist
            onClicked: appController.playTrackById(trackId)
        }
    }
}