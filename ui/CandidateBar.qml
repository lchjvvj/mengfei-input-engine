import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Window 6.0

Window {
    id: candidateWin
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    visible: false
    width: 420
    height: 64
    color: "#F7F7F7"

    // 候选栏位置，由C++侧从gxde-wlcom获取光标坐标并设置
    property real cursorX: 0
    property real cursorY: 0

    // 预编辑文本 + 候选词列表
    property string preeditText: ""
    property list<string> candidates: []
    property int selectedIndex: 0

    // 主题切换，读取GXDE系统主题
    property bool darkMode: false
    color: darkMode ? "#2E2E2E" : "#FFFFFF"

    onCursorXChanged: {
        x = cursorX
        y = cursorY + 24
        // 边界判断，防止候选栏超出屏幕
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
                    onClicked: {
                        // 点击候选词，触发选词信号，回传给C++引擎
                        candidateBarCpp.selectCandidate(index)
                    }
                }
            }
        }
    }

    // 对外暴露C++对象，QML调用C++槽函数
    Connections {
        target: candidateBarCpp
        function onHide() { candidateWin.visible = false }
        function onShow() { candidateWin.visible = true }
    }
}
