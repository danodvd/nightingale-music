import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: sidebar
    color: "#18181c"

    property int activeIndex: 0
    signal pageSelected(int index)

    // Componente interno reutilizable para cada botón de sección
    component NavButton: Rectangle {
        required property int pageIndex
        required property string iconGlyph
        required property string label

        readonly property bool isActive: sidebar.activeIndex === pageIndex

        Layout.fillWidth: true
        implicitHeight: 40
        radius: 8
        color: isActive ? "#27272f" : (mouseArea.containsMouse ? "#202026" : "transparent")

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            NerdIcon {
                glyph: iconGlyph
                size: 18
                iconColor: isActive ? "#f4f4f6" : "#a1a1aa"
            }

            Label {
                text: label
                color: isActive ? "#f4f4f6" : "#a1a1aa"
                font.bold: isActive
                font.pixelSize: 14
                Layout.fillWidth: true
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: sidebar.pageSelected(pageIndex)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 6

        Label {
            text: "NIGHTINGALE"
            color: "#f4f4f6"
            font.pixelSize: 16
            font.bold: true
            Layout.bottomMargin: 10
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 2
            color: "#f4f4f6"
            anchors.topMargin: 10
            anchors.bottomMargin: 10
        }

        NavButton { pageIndex: 0; iconGlyph: "\uf015"; label: "Home" }
        NavButton { pageIndex: 1; iconGlyph: "\uf51f"; label: "Albums" }
        NavButton { pageIndex: 2; iconGlyph: "\uf007"; label: "Artists" }
        NavButton { pageIndex: 3; iconGlyph: "\udb84\udf59"; label: "Library" }

        Item { Layout.fillHeight: true }

        NavButton { pageIndex: 4; iconGlyph: "\uf013"; label: "Settings" }
    }
}