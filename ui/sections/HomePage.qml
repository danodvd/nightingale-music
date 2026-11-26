import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Flickable {
    id: homeFlickable
    contentWidth: width
    contentHeight: homeColumn.implicitHeight + 24
    clip: true

    ScrollBar.vertical: ScrollBar {}

    component HorizontalTrackSection: ColumnLayout {
        required property string sectionTitle
        required property var sectionModel

        Layout.fillWidth: true
        spacing: 10

        Label {
            text: sectionTitle
            color: "#f4f4f6"
            font.pixelSize: 18
            font.bold: true
        }

        ListView {
            Layout.fillWidth: true
            implicitHeight: 188
            orientation: ListView.Horizontal
            spacing: 12
            clip: true
            model: sectionModel

            delegate: TrackCard {
                w: 140
                h: 188
                imageRadius: 6
            }
        }
    }

    ColumnLayout {
        id: homeColumn
        width: homeFlickable.width
        spacing: 24

        HorizontalTrackSection {
            sectionTitle: "Escuchadas recientemente"
            sectionModel: appController.recentTracksModel
        }

        HorizontalTrackSection {
            sectionTitle: "Canciones más escuchadas"
            sectionModel: appController.topTracksModel
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "Artistas más escuchados"
                color: "#f4f4f6"
                font.pixelSize: 18
                font.bold: true
            }

            ListView {
                Layout.fillWidth: true
                implicitHeight: 156
                orientation: ListView.Horizontal
                spacing: 14
                clip: true
                model: appController.topArtistsModel

                delegate: ColumnLayout {
                    required property string name
                    required property string coverUrl
                    required property int playCount

                    width: 116
                    spacing: 6

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 104
                        height: 104
                        radius: 52
                        color: "#27272a"
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: coverUrl
                            fillMode: Image.PreserveAspectCrop
                            visible: coverUrl !== ""
                        }

                        NerdIcon {
                            anchors.centerIn: parent
                            glyph: "\uf007"
                            size: 32
                            iconColor: "#71717a"
                            visible: coverUrl === ""
                        }
                    }

                    Label {
                        text: name
                        color: "#f4f4f6"
                        font.bold: true
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }

                    Label {
                        text: playCount + " reprod."
                        color: "#71717a"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Canciones Favoritas (lista completa)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "Canciones favoritas"
                color: "#f4f4f6"
                font.pixelSize: 18
                font.bold: true
            }

            Repeater {
                model: appController.favoriteTracksModel

                delegate: Rectangle {
                    required property var trackId
                    required property string title
                    required property string artist
                    required property string durationText
                    required property string coverUrl

                    Layout.fillWidth: true
                    implicitHeight: 52
                    radius: 6
                    color: favMouse.containsMouse ? "#232329" : "#18181c"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12

                        Rectangle {
                            width: 38
                            height: 38
                            radius: 6
                            color: "#27272a"
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: coverUrl
                                fillMode: Image.PreserveAspectCrop
                                visible: coverUrl !== ""
                            }
                        }

                        Label {
                            text: title
                            color: "#f4f4f6"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: artist
                            color: "#a1a1aa"
                            elide: Text.ElideRight
                            Layout.preferredWidth: 180
                        }

                        NerdIcon {
                            glyph: "\uf004"
                            size: 16
                            iconColor: "#f43f5e"

                            MouseArea {
                                anchors.fill: parent
                                onClicked: appController.toggleFavorite(trackId)
                            }
                        }

                        Label {
                            text: durationText
                            color: "#71717a"
                            Layout.preferredWidth: 46
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    MouseArea {
                        id: favMouse
                        anchors.fill: parent
                        z: -1
                        hoverEnabled: true
                        onClicked: appController.playTrackById(trackId)
                    }
                }
            }
        }
    }
}