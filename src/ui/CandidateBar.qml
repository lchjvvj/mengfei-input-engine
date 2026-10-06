import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Window 6.0

Window {
    id: candidateWin
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    visible: false
    width: 520
    height: 64
    // 主题与样式绑定 GXDESettings（settingsCpp 上下文属性，由 CandidateBar C++ 注入）：
    // 设置面板修改深浅色/字号/间距/页码/高亮色后即时生效
    color: settingsCpp.darkMode ? "#2B2B2B" : "#FFFFFF"
    property real cursorX: 0
    property real cursorY: 0
    property string preeditText: ""
    property list<string> candidates: []
    property int selectedIndex: 0
    property int currentPage: 0
    property int totalPages: 0

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
        spacing: settingsCpp.barSpacing
        padding: 8

        Text {
            text: preeditText
            font.pixelSize: settingsCpp.barFontSize + 2
            color: settingsCpp.darkMode ? "#EEEEEE" : "#111111"
        }

        Repeater {
            model: candidates
            delegate: Row {
                spacing: 4
                Text {
                    text: (index + 1) + "."
                    font.bold: true
                    font.pixelSize: settingsCpp.barFontSize
                    color: selectedIndex === index ? settingsCpp.accentColor : "#666666"
                }
                Text {
                    text: modelData
                    font.pixelSize: settingsCpp.barFontSize
                    color: selectedIndex === index ? settingsCpp.accentColor
                                                   : settingsCpp.darkMode ? "#DDDDDD" : "#222222"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: candidateBarCpp.selectCandidate(index)
                }
            }
        }
        // 页码提示：设置开启"显示候选页码"且多页候选时显示 1/2 页码
        Text {
            text: currentPage + "/" + totalPages
            font.pixelSize: settingsCpp.barFontSize - 2
            color: "#888"
            visible: settingsCpp.showPageInfo && totalPages > 1
        }
    }

    Connections {
        target: candidateBarCpp
        function onHide() { candidateWin.visible = false }
        function onShow() { candidateWin.visible = true }
    }
}
