// sections/views/PlaylistSubView.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
ColumnLayout {
    id: playlistSubView

    required property var playlistId
    property string coverUrl: ""
    property string subtitle: ""
    required property string inViewName
    required property int trackCount
    required property var tracksModel

    signal backClicked()
    signal playAllClicked()
    signal trackClicked(var trackId, int index)

    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Button {
            text: "Renombrar"
            onClicked: renameDialog.open()
        }

        Button {
            text: "Cambiar portada"
            onClicked: coverFileDialog.open()
        }

        Button {
            text: "+ Añadir canciones"
            onClicked: addTracksDialog.open()
        }

        Item { Layout.fillWidth: true }

        Button {
            text: "Eliminar playlist"
            onClicked: {
                if (playlistSubView.playlistId > 0) {
                    appController.deletePlaylist(playlistSubView.playlistId)
                    playlistSubView.backClicked()
                }
            }
        }
    }

    InSubView {
        Layout.fillWidth: true
        Layout.fillHeight: true

        inViewName: playlistSubView.inViewName
        subtitle: playlistSubView.subtitle
        coverUrl: playlistSubView.coverUrl
        trackCount: playlistSubView.trackCount
        tracksModel: playlistSubView.tracksModel
        circularCover: false

        onBackClicked: playlistSubView.backClicked()
        onPlayAllClicked: playlistSubView.playAllClicked()
        onTrackClicked: (trackId, index) => playlistSubView.trackClicked(trackId, index)

        Dialog {
            id: t
            title: "Renombrar playlist"
            anchors.centerIn: Overlay.overlay
            modal: true
            standardButtons: Dialog.Ok | Dialog.Cancel

            TextField {
                id: renameInput
                text: playlistSubView.inViewName
                implicitWidth: 260
            }

            onAccepted: {
                if (renameInput.text.trim().length > 0) {
                    appController.renamePlaylist(playlistSubView.playlistId, renameInput.text.trim())
                }
            }
        }

        FileDialog {
            id: coverFileDialog
            title: "Seleccionar portada de playlist"
            nameFilters: ["Imágenes (*.png *.jpg *.jpeg *.webp)"]
            onAccepted: appController.setPlaylistCover(playlistSubView.playlistId, selectedFile)
        }

        Dialog {
            id: addTracksDialog
            title: "Añadir canciones"
            anchors.centerIn: Overlay.overlay
            modal: true
            width: 420
            height: 460
            standardButtons: Dialog.Close

            ListView {
                anchors.fill: parent
                clip: true
                model: appController.tracksModel

                delegate: ItemDelegate {
                    width: ListView.view.width
                    required property var trackId
                    required property string title
                    required property string artist

                    text: title + " — " + artist
                    onClicked: {
                        if (playlistSubView.playlistId > 0 && trackId > 0) {
                            appController.addTrackToPlaylist(playlistSubView.playlistId, trackId)
                            appController.openPlaylistSubView(playlistSubView.playlistId)
                        }
                    }
                }
            }
        }
    }
}