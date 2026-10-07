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
    // text-input-v3 光标矩形（gxde-wlcom 换算为全局坐标后经
    // cursorRectChanged -> setCursorRect 写入），候选栏跟随定位
    property real cursorX: 0
    property real cursorY: 0
    property real cursorHeight: 0
    // 空矩形（应用未上报光标区域）时的默认行高偏移，避免遮住光标点
    readonly property real fallbackCursorOffset: 24
    property string preeditText: ""
    property list<string> candidates: []
    property int selectedIndex: 0
    property int currentPage: 0
    property int totalPages: 0

    // 按光标矩形左下角定位：候选栏紧贴矩形下缘，不遮挡光标行；
    // 下方放不下时翻转到光标上方，横向超出时收回屏幕内
    function reposition() {
        var px = cursorX
        var py = cursorY + (cursorHeight > 0 ? cursorHeight : fallbackCursorOffset)
        if (px + width > Screen.width)
            px = Screen.width - width
        if (py + height > Screen.height)
            py = cursorY - height
        x = px
        y = py
    }

    // 任一坐标分量变化都要跟随（修复仅横向移动触发、纵向移动不跟随的问题）
    onCursorXChanged: reposition()
    onCursorYChanged: reposition()
    onCursorHeightChanged: reposition()

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
