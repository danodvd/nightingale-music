import QtQuick

Text {
    id: iconRoot

    property string glyph: ""
    property real size: 18
    property color iconColor: "#f4f4f6"

    FontLoader {
        id: nerdFont
        source: Qt.resolvedUrl("../fonts/SymbolsNerdFont-Regular.ttf")
    }

    text: iconRoot.glyph
    font.family: nerdFont.name
    font.pixelSize: iconRoot.size
    color: iconRoot.iconColor
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    renderType: Text.NativeRendering
}