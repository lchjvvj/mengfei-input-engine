Window {
    id: candidateWin
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    visible: false
    width: 520
    height: 64
    property real cursorX: 0
    property real cursorY: 0
    property string preeditText: ""
    property list<string> candidates: []
    property int selectedIndex: 0
    property bool darkMode: false

    onCursorXChanged: {
        x = cursorX
        y = cursorY + 24
        if (x + width > Screen.width)
            x = Screen.width - width
        if (y + height > Screen.height)
            y = cursorY - height
    }

    Row {
        anchors.fill: parent
        spacing: 8
        padding: 8

        Text {
            text: preeditText
            font.pixelSize: 18
            color: darkMode ? "#EEEEEE" : "#111111"
        }

        Repeater {
            model: candidates
            delegate: Row {
                spacing: 4
                Text {
                    text: (index + 1) + "."
                    font.bold: true
                    color: selectedIndex === index ? "#0078D4" : "#666666"
                }
                Text {
                    text: modelData
                    font.pixelSize: 16
                    color: selectedIndex === index ? "#0078D4" : darkMode ? "#DDDDDD" : "#222222"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: candidateBarCpp.selectCandidate(index)
                }
            }
        }
        // 页码提示
        Text {
            text: "Pg"
            font.pixelSize:14
            color: "#888"
        }
    }

    Connections {
        target: candidateBarCpp
        function onHide() { candidateWin.visible = false }
        function onShow() { candidateWin.visible = true }
    }
}
