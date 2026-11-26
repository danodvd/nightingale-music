import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import "../components/"
import "views/"

ColumnLayout {
    id: libraryPage
    property int currentTab: 0
    spacing: 14

    signal playlistSelected(var playlistId)
    signal createPlaylistRequested()

    RowLayout {
        spacing: 8

        Repeater {
            model: ["Songs", "Playlists"]

            delegate: Rectangle {
                required property int index
                required property string modelData

                implicitWidth: tabLabel.implicitWidth + 28
                implicitHeight: 34
                radius: 8
                color: libraryPage.currentTab === index
                    ? "#2563eb"
                    : (tabMouse.containsMouse ? "#232329" : "#18181c")

                Text {
                    id: tabLabel
                    anchors.centerIn: parent
                    text: modelData
                    color: libraryPage.currentTab === index ? "#ffffff" : "#a1a1aa"
                    font.pixelSize: 13
                    font.bold: libraryPage.currentTab === index
                }

                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: libraryPage.currentTab = index
                }
            }
        }

        Item { Layout.fillWidth: true }

        Button {
            text: "+ New playlist"
            visible: libraryPage.currentTab === 1
            onClicked: libraryPage.createPlaylistRequested()
        }
    }

    StackLayout {
        Layout.fillHeight: true
        Layout.fillWidth: true
        currentIndex: libraryPage.currentTab

        SongsView {}
        PlaylistsView {
            onPlaylistSelected: {
                libraryPage.playlistSelected(playlistId)
            }
        }
    }
}