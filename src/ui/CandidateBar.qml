import QtQuick 6.0
import QtQuick.Window 6.0

// 候选栏UI：根项为普通 Rectangle（QQuickView 不支持 Window 作根项，
// Window 作根会导致 setSource 加载失败、rootObject 为空）。
// 窗口 flags（无边框/置顶/Tool）与显示隐藏由 CandidateBar C++ 在 QQuickView 上控制。
Rectangle {
    id: candidateRoot
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
    // 下方放不下时翻转到光标上方，横向超出时收回屏幕内。
    // 根项是普通 Item，其 x/y 只在视图内布局；窗口位置需通过
    // Window.window 附加属性写到宿主 QQuickView 窗口上
    function reposition() {
        var px = cursorX
        var py = cursorY + (cursorHeight > 0 ? cursorHeight : fallbackCursorOffset)
        if (px + width > Screen.width)
            px = Screen.width - width
        if (py + height > Screen.height)
            py = cursorY - height
        var win = Window.window
        if (!win)
            return
        win.x = px
        win.y = py
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
}
