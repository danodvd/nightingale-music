import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import "../../components/"

ColumnLayout {
    id: root

    property string coverUrl: ""
    property string subtitle: ""          // Ej: "Álbum", "Artista", "Playlist" o nombre del artista
    property bool circularCover: false    // true para artistas, false para álbumes/playlists
    required property string inViewName
    required property int trackCount
    required property var tracksModel

    signal backClicked()
    signal playAllClicked()
    signal trackClicked(var trackId, int index)

    spacing: 16

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 196
        radius: 12
        color: "#18181c"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // Botón volver
            Rectangle {
                implicitWidth: backRow.implicitWidth + 16
                implicitHeight: 28
                radius: 6
                color: backMouse.containsMouse ? "#27272f" : "transparent"

                RowLayout {
                    id: backRow
                    anchors.centerIn: parent
                    spacing: 6

                    NerdIcon {
                        glyph: "\uf060" // Flecha izquierda
                        size: 13
                        iconColor: "#a1a1aa"
                    }

                    Text {
                        text: "Back"
                        color: "#a1a1aa"
                        font.pixelSize: 13
                    }
                }

                MouseArea {
                    id: backMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.backClicked()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 18

                Rectangle {
                    id: coverContainer
                    Layout.preferredWidth: 128
                    Layout.preferredHeight: 128
                    radius: root.circularCover ? 64 : 10
                    color: "#27272a"

                    Image {
                        id: coverImg
                        anchors.fill: parent
                        source: root.coverUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        visible: root.coverUrl !== ""

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

                    NerdIcon {
                        anchors.centerIn: parent
                        glyph: root.circularCover ? "\uf007" : "\uf001"
                        size: 36
                        iconColor: "#71717a"
                        visible: root.coverUrl === ""
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 6

                    Text {
                        text: root.subtitle
                        font.pixelSize: 12
                        font.bold: true
                        color: "#a1a1aa"
                        visible: root.subtitle !== ""
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.inViewName
                        font.pixelSize: 26
                        font.bold: true
                        color: "#f4f4f6"
                        elide: Text.ElideRight
                    }

                    Text {
                        text: root.trackCount + (root.trackCount === 1 ? " song" : " songs")
                        font.pixelSize: 13
                        color: "#8e8e8e"
                    }

                    Item { Layout.fillHeight: true }

                    Rectangle {
                        implicitWidth: playRow.implicitWidth + 28
                        implicitHeight: 36
                        radius: 18
                        color: playMouse.containsMouse ? "#3b82f6" : "#2563eb"
                        opacity: root.trackCount > 0 ? 1.0 : 0.4

                        RowLayout {
                            id: playRow
                            anchors.centerIn: parent
                            spacing: 8

                            NerdIcon {
                                glyph: "\uf04b" // Icono Play
                                size: 13
                                iconColor: "#ffffff"
                            }

                            Text {
                                text: "Play"
                                color: "#ffffff"
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }

                        MouseArea {
                            id: playMouse
                            anchors.fill: parent
                            enabled: root.trackCount > 0
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.playAllClicked()
                        }
                    }
                }
            }
        }
    }

    ListView {
        id: tracksView
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: root.tracksModel
        spacing: 4

        ScrollBar.vertical: ScrollBar {}

        delegate: Rectangle {
            required property int index
            required property var trackId
            required property string title
            required property string artist
            required property string durationText
            required property string coverUrl

            width: tracksView.width
            height: 50
            radius: 8
            color: rowMouse.containsMouse ? "#232329" : "#18181c"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                Text {
                    text: (index + 1)
                    color: "#71717a"
                    font.pixelSize: 13
                    Layout.preferredWidth: 24
                    horizontalAlignment: Text.AlignRight
                }

                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: 6
                    color: "#27272a"
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: coverUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        visible: coverUrl !== ""
                    }

                    NerdIcon {
                        anchors.centerIn: parent
                        glyph: "\uf001"
                        size: 14
                        iconColor: "#71717a"
                        visible: coverUrl === ""
                    }
                }

                Text {
                    text: title
                    color: "#f4f4f6"
                    font.pixelSize: 14
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: artist
                    color: "#a1a1aa"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    Layout.preferredWidth: 180
                }

                Text {
                    text: durationText
                    color: "#71717a"
                    font.pixelSize: 13
                    Layout.preferredWidth: 46
                    horizontalAlignment: Text.AlignRight
                }
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {root.trackClicked(trackId, index)}
            }
        }
    }
}