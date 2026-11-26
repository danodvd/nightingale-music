import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import "components/" 
import "fonts/"
import "sections/"
import "sections/views"

ApplicationWindow {
    id: root
    width: 1120
    height: 720
    minimumWidth: 860
    minimumHeight: 560
    visible: true
    title: "Nightingale"
    color: "#121214"

    property int currentPage: 0
    property int previousPage: 0
    property bool inSubView: false
    property bool subViewCircular: false

    property string subViewType: "" // "album", "artist" or "playlist"
    property var currentPlaylistId: 0

    function showPlaylistSubView(playlistId) {
        currentPlaylistId = playlistId
        subViewType = "playlist"
        subViewCircular = false
        appController.openPlaylistSubView(playlistId)
        inSubView = true
    }

    FolderDialog {
        id: folderDialog
        title: "Seleccionar carpeta de música"
        onAccepted: appController.addLibraryFolder(selectedFolder)
    }

    Dialog {
        id: newPlaylistDialog
        title: "Nueva playlist"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        TextField {
            id: playlistNameInput
            placeholderText: "Playlist Name"
            implicitWidth: 260
        }

        onAccepted: {
            if (playlistNameInput.text.trim().length > 0) {
                appController.createPlaylist(playlistNameInput.text.trim())
                playlistNameInput.text = ""
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 240
                Layout.fillHeight: true
                color: "#18181c"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Sidebar {
                        Layout.preferredWidth: 220
                        Layout.fillHeight: true
                        activeIndex: root.currentPage
                        onPageSelected: (index) => {
                            root.inSubView = false
                            root.currentPage = index
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#121214"
                clip: true

                StackLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    currentIndex: root.currentPage
                    visible: !root.inSubView

                    HomePage {}     
                    AlbumsPage {}    
                    ArtistsPage {}  
                    LibraryPage {
                        onPlaylistSelected: {
                            root.showPlaylistSubView(playlistId)
                        }

                        onCreatePlaylistRequested: {
                            newPlaylistDialog.open()
                        }
                    }
                    SettingsPage {} 
                }

                InSubView {
                    anchors.fill: parent
                    anchors.margins: 18
                    visible: root.inSubView && root.subViewType !== "playlist"

                    inViewName: appController.subViewTitle
                    subtitle: appController.subViewSubtitle
                    coverUrl: appController.subViewCover
                    trackCount: appController.subViewTrackCount
                    tracksModel: appController.subViewTracksModel
                    circularCover: root.subViewCircular

                    onBackClicked: root.inSubView = false
                    onPlayAllClicked: appController.playSubViewAll(0)
                    onTrackClicked: (trackId, index) => appController.playSubViewAll(index)
                }

                PlaylistSubView {
                    anchors.fill: parent
                    anchors.margins: 18
                    visible: root.inSubView && root.subViewType === "playlist"

                    playlistId: root.currentPlaylistId
                    inViewName: appController.subViewTitle
                    subtitle: appController.subViewSubtitle
                    coverUrl: appController.subViewCover
                    trackCount: appController.subViewTrackCount
                    tracksModel: appController.subViewTracksModel

                    onBackClicked: {
                        root.inSubView = false
                        root.subViewType = ""
                    }
                    onPlayAllClicked: appController.playSubViewAll(0)
                    onTrackClicked: (trackId, index) => appController.playSubViewAll(index)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            color: "#1c1c21"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 16

                RowLayout {
                    Layout.preferredWidth: 260
                    spacing: 12

                    Rectangle {
                        width: 52
                        height: 52
                        radius: 6
                        color: "#27272a"
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: appController.currentTrackCover
                            fillMode: Image.PreserveAspectCrop
                            visible: appController.currentTrackCover !== ""
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: appController.currentTrackTitle
                            color: "#f4f4f6"
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: appController.currentTrackArtist
                            color: "#a1a1aa"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        spacing: 40

                        PlaybackButton {
                            Layout.alignment: Qt.AlignVCenter
                            w: 16
                            h: 16
                            r: 8
                            icon: "\uf074"
                            textColor: appController.shuffleEnabled ? '#c8c847' : "#f4f4f6"
                            fontSize: 16
                            callback: function() {
                                appController.toggleShuffle()
                            }
                        }

                        PlaybackButton {
                            Layout.alignment: Qt.AlignVCenter
                            w: 24
                            h: 24
                            r: 12
                            icon: "\udb81\udcae"
                            callback: function() {
                                appController.previousTrack()
                            }
                        }

                        PlaybackButton {
                            Layout.alignment: Qt.AlignVCenter
                            w: 32
                            h: 32
                            r: 16
                            icon: appController.isPlaying ? "\uf28b" : "\uf144"
                            callback: function() {
                                appController.togglePlayPause()
                            }
                        }

                        PlaybackButton {
                            Layout.alignment: Qt.AlignVCenter
                            w: 24
                            h: 24
                            r: 16
                            icon: "\udb81\udcad"
                            callback: function() {
                                appController.nextTrack()
                            }
                        }

                        PlaybackButton {
                            Layout.alignment: Qt.AlignVCenter
                            w: 16
                            h: 16
                            r: 8
                            fontSize: 16
                            textColor: appController.repeatMode !== 0 ? "#c8c847" : "#f4f4f6"
                            icon: appController.repeatMode === 1 ? "󰑘" : "󰑖"
                            callback: function() {
                                appController.cycleRepeatMode()
                            }
                        }
                    }

                    Slider {
                        id: seekSlider
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(1, appController.durationMs)
                        value: pressed ? value : appController.positionMs
                        onMoved: appController.seekTo(Math.round(value))
                    }
                }

                RowLayout {
                    Layout.preferredWidth: 160
                    spacing: 8

                    Label {
                        text: "Vol"
                        color: "#a1a1aa"
                    }

                    Slider {
                        Layout.fillWidth: true
                        from: 0.0
                        to: 1.0
                        value: appController.volume
                        onMoved: appController.setVolume(value)
                    }
                }
            }
        }
    }
}