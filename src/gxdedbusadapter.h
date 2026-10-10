#ifndef GXDEDBUSADAPTER_H
#define GXDEDBUSADAPTER_H

#include <QObject>

class SettingsPanel;

// DBus 辅助通道：主输入链路（激活/按键/候选窗/文本回写）已全部由
// GXDEWaylandIM 走标准 zwp_input_method_v2 直连 gxde-wlcom 完成
//（候选窗经 zwp_input_popup_surface_v2 显示），旧的整套 DBus 按键协议
//（Activate/Deactivate/Reset/ProcessKeyEvent/SetCursorPos/SetCursorRect
// 与 PreeditChanged/CandidateListChanged/CommitText/PageInfoChanged
// 出站信号）随之删除。这里仅保留 GXDE 控制中心等组件打开设置面板的
// 管理入口（ShowSettings）。
// 服务名与 debian/gxde-input-engine.user.service 的 BusName=org.gxde.InputEngine
// 保持一致（session bus，systemd user 服务）；注册结果同时充当
// 单实例守护（名字被占用说明已有引擎在运行）。
class GXDEDBusAdapter : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.gxde.InputEngine")
public:
    static constexpr const char *kServiceName = "org.gxde.InputEngine";
    static constexpr const char *kObjectPath = "/org/gxde/InputEngine";

    explicit GXDEDBusAdapter(SettingsPanel *settingsPanel, QObject *parent = nullptr);

    // 在 session bus 上注册对象与服务名；失败返回 false（总线不可用/名字被占用）
    bool registerService();

public slots:
    // 打开输入法设置面板（中英文切换、候选栏样式、主题配置），
    // 供 GXDE 控制中心/合成器调用；面板常驻隐藏，重复调用幂等
    void ShowSettings();

private:
    SettingsPanel *m_settingsPanel;
};

#endif // GXDEDBUSADAPTER_H
