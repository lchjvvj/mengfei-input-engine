import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0

// 输入法设置面板：中英文切换、候选栏样式、主题配置。
// 所有控件直接读写 settingsCpp（GXDESettings）属性：修改即持久化，
// 候选栏 QML 绑定同一对象，样式与主题即时生效。
// 根项为普通 Item（QQuickView 不支持 Window 作根项），窗口标题由 C++ 设置。
Rectangle {
    id: settingsRoot
    width: 460
    height: 640
    color: palette.window

    // 调色板跟随深色主题开关，Basic 风格控件与 Label 自动取色
    palette.window: settingsCpp.darkMode ? "#1E1E1E" : "#F6F6F6"
    palette.windowText: settingsCpp.darkMode ? "#EEEEEE" : "#111111"
    palette.base: settingsCpp.darkMode ? "#2B2B2B" : "#FFFFFF"
    palette.text: settingsCpp.darkMode ? "#EEEEEE" : "#111111"
    palette.button: settingsCpp.darkMode ? "#383838" : "#E6E6E6"
    palette.buttonText: settingsCpp.darkMode ? "#EEEEEE" : "#111111"
    palette.highlight: settingsCpp.accentColor
    palette.highlightedText: "#FFFFFF"

    // 外部修改设置（如"恢复默认"）后把控件同步回设置值；
    // 拖动/点击会命令式修改控件状态并断开声明式绑定，需显式回写
    Connections {
        target: settingsCpp
        function onChineseEnabledChanged(enabled) { chineseSwitch.checked = enabled }
        function onShowPageInfoChanged(show) { pageInfoSwitch.checked = show }
        function onDarkModeChanged(dark) {
            lightRadio.checked = !dark
            darkRadio.checked = dark
        }
        function onBarFontSizeChanged(px) { fontSlider.value = px }
        function onBarSpacingChanged(spacing) { spacingSlider.value = spacing }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        clip: true

        ColumnLayout {
            id: contentColumn
            width: parent.width
            spacing: 16

            Label {
                text: "输入法设置"
                font.pixelSize: 24
                font.bold: true
            }

            // ---- 输入：中英文切换 ----
            GroupBox {
                Layout.fillWidth: true
                title: "输入"

                ColumnLayout {
                    width: parent.width
                    spacing: 4

                    Switch {
                        id: chineseSwitch
                        text: "中文输入"
                        checked: settingsCpp.chineseEnabled
                        onToggled: settingsCpp.chineseEnabled = checked
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "关闭后为英文模式：按键不再被输入法消费，字符直接上屏，不弹出候选栏"
                        font.pixelSize: 12
                        opacity: 0.6
                        wrapMode: Text.Wrap
                    }
                }
            }

            // ---- 候选栏样式 ----
            GroupBox {
                Layout.fillWidth: true
                title: "候选栏样式"

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "字体大小"; Layout.preferredWidth: 64 }
                        Slider {
                            id: fontSlider
                            Layout.fillWidth: true
                            from: 12
                            to: 28
                            stepSize: 1
                            value: settingsCpp.barFontSize
                            onMoved: settingsCpp.barFontSize = Math.round(value)
                        }
                        Label {
                            Layout.preferredWidth: 48
                            text: Math.round(fontSlider.value) + " px"
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "候选间距"; Layout.preferredWidth: 64 }
                        Slider {
                            id: spacingSlider
                            Layout.fillWidth: true
                            from: 2
                            to: 24
                            stepSize: 2
                            value: settingsCpp.barSpacing
                            onMoved: settingsCpp.barSpacing = Math.round(value)
                        }
                        Label {
                            Layout.preferredWidth: 48
                            text: Math.round(spacingSlider.value) + " px"
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    Switch {
                        id: pageInfoSwitch
                        text: "显示候选页码"
                        checked: settingsCpp.showPageInfo
                        onToggled: settingsCpp.showPageInfo = checked
                    }

                    // 实时预览：与 CandidateBar.qml 相同的渲染规则，
                    // 拖动滑块/切换主题立即可见效果
                    Rectangle {
                        Layout.fillWidth: true
                        height: previewRow.implicitHeight + 16
                        radius: 4
                        color: settingsCpp.darkMode ? "#2B2B2B" : "#FFFFFF"
                        border.color: settingsCpp.darkMode ? "#3C3C3C" : "#DDDDDD"

                        Row {
                            id: previewRow
                            x: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: settingsCpp.barSpacing

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "ni hao"
                                font.pixelSize: settingsCpp.barFontSize + 2
                                color: settingsCpp.darkMode ? "#EEEEEE" : "#111111"
                            }
                            Repeater {
                                model: ["你好", "你", "呢"]
                                delegate: Row {
                                    spacing: 4
                                    Text {
                                        text: (index + 1) + "."
                                        font.bold: true
                                        font.pixelSize: settingsCpp.barFontSize
                                        color: index === 0 ? settingsCpp.accentColor : "#666666"
                                    }
                                    Text {
                                        text: modelData
                                        font.pixelSize: settingsCpp.barFontSize
                                        color: index === 0 ? settingsCpp.accentColor
                                                           : settingsCpp.darkMode ? "#DDDDDD" : "#222222"
                                    }
                                }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "1/2"
                                font.pixelSize: settingsCpp.barFontSize - 2
                                color: "#888888"
                                visible: settingsCpp.showPageInfo
                            }
                        }
                    }
                }
            }

            // ---- 主题配置 ----
            GroupBox {
                Layout.fillWidth: true
                title: "主题"

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    RowLayout {
                        spacing: 16
                        Label { text: "外观"; Layout.preferredWidth: 64 }
                        RadioButton {
                            id: lightRadio
                            text: "浅色"
                            checked: !settingsCpp.darkMode
                            onClicked: settingsCpp.darkMode = false
                        }
                        RadioButton {
                            id: darkRadio
                            text: "深色"
                            checked: settingsCpp.darkMode
                            onClicked: settingsCpp.darkMode = true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Label { text: "高亮色"; Layout.preferredWidth: 64 }
                        Repeater {
                            model: ["#0078D4", "#E81123", "#CA5010",
                                    "#107C10", "#00B7C3", "#8764B8"]
                            delegate: Rectangle {
                                id: accentDot
                                required property string modelData
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                radius: 14
                                color: modelData
                                border.width: settingsCpp.accentColor === modelData ? 3 : 1
                                border.color: settingsCpp.darkMode ? "#FFFFFF" : "#333333"

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: settingsCpp.accentColor = accentDot.modelData
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // ---- 底部操作 ----
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: "恢复默认"
                    onClicked: settingsCpp.resetDefaults()
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "关闭"
                    onClicked: settingsPanelCpp.close()
                }
            }
        }
    }
}
