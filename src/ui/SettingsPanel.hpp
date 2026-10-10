#ifndef SETTINGSPANEL_HPP
#define SETTINGSPANEL_HPP

#include <QObject>
#include <QQuickView>

class GXDESettings;

// 输入法设置面板（QtQuick）：中英文切换、候选栏样式、主题配置。
// QQuickView 承载 qrc 内的 QML，QML 根项使用普通 Item（QQuickView
// 不支持 Window 作根项）。候选栏已改经 zwp_input_popup_surface_v2
// 显示（见 CandidateBar/GXDEWaylandIM），设置面板仍是普通窗口。
// 设置数据通过 settingsCpp 上下文属性直接读写 GXDESettings：
// 修改即持久化，候选栏（同样绑定 settingsCpp）即时生效。
// 面板由 DBus 方法 ShowSettings 打开（GXDE 控制中心/合成器调用），
// 平时隐藏，不随引擎按键链路参与。
class SettingsPanel : public QObject
{
    Q_OBJECT
public:
    explicit SettingsPanel(GXDESettings *settings, QObject *parent = nullptr);
    ~SettingsPanel() override;

public slots:
    // 显示面板并置于前台；已显示时仅提升焦点（幂等）
    void show();
    // 关闭面板（隐藏窗口，不销毁），供 QML"关闭"按钮与 C++ 侧调用
    void close();

private:
    QQuickView *m_view;
};

#endif // SETTINGSPANEL_HPP
