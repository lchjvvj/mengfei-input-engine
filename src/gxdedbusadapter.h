#ifndef GXDEDBUSADAPTER_H
#define GXDEDBUSADAPTER_H

#include <QObject>
#include <QStringList>

class GXDEInputCore;
class GXDEPinyinEngine;
class CandidateBar;
class SettingsPanel;

// DBus 双向通信适配层：gxde-wlcom 合成器与输入引擎之间的唯一通道
// 入站（方法调用，合成器 -> 引擎）：
//   Activate / Deactivate / Reset / ProcessKeyEvent / SetCursorPos /
//   SetCursorRect / ShowSettings
// 出站（信号广播，引擎 -> 合成器）：
//   PreeditChanged / CandidateListChanged / CommitText / PageInfoChanged
// 服务名与 debian/gxde-input-engine.service 的 BusName=org.gxde.InputEngine
// 保持一致（session bus，systemd user 服务）；接口描述见
// protocol/org.gxde.InputEngine.xml，gxde-wlcom 侧可用 qdbusxml2cpp 生成代理。
class GXDEDBusAdapter : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.gxde.InputEngine")
public:
    static constexpr const char *kServiceName = "org.gxde.InputEngine";
    static constexpr const char *kObjectPath = "/org/gxde/InputEngine";

    explicit GXDEDBusAdapter(GXDEInputCore *core, GXDEPinyinEngine *pinyin,
                             CandidateBar *ui, SettingsPanel *settingsPanel,
                             QObject *parent = nullptr);

    // 在 session bus 上注册对象与服务名；失败返回 false（总线不可用/名字被占用）
    bool registerService();

public slots:
    // 对应 text_input enter/leave：激活/关闭输入上下文
    void Activate();
    void Deactivate();
    // 对应 text_input reset：清空预编辑与候选，保持上下文激活
    void Reset();
    // 按键转发：evdev raw keycode + 按下状态，返回引擎是否消费该按键
    bool ProcessKeyEvent(uint keycode, bool pressed);
    // 光标/输入框全局坐标（点形式），候选栏跟随定位（由 gxde-wlcom 实时推送），
    // 等价于空矩形的 SetCursorRect，候选栏回退默认行高偏移
    void SetCursorPos(int x, int y);
    // text-input-v3 set_cursor_rectangle 光标矩形（gxde-wlcom 换算为全局坐标
    // 后推送），候选栏紧贴矩形左下角定位，不遮挡光标行
    void SetCursorRect(int x, int y, int width, int height);
    // 打开输入法设置面板（中英文切换、候选栏样式、主题配置），
    // 供 GXDE 控制中心/合成器调用；面板常驻隐藏，重复调用幂等
    void ShowSettings();

signals:
    // 预编辑文本更新，cursor 为光标位置（当前实现固定在文本末尾）
    void PreeditChanged(const QString &text, int cursor);
    // 当前页候选词列表与选中索引
    void CandidateListChanged(const QStringList &candidates, int selected);
    // 上屏文本：合成器收到后通过 zwp_text_input_v3.commit_string 提交给应用
    void CommitText(const QString &text);
    // 页码信息（1 起始，无候选时均为 0）
    void PageInfoChanged(int currentPage, int totalPages);

private:
    GXDEInputCore *m_core;
    GXDEPinyinEngine *m_pinyin;
    CandidateBar *m_ui;
    SettingsPanel *m_settingsPanel;
};

#endif // GXDEDBUSADAPTER_H
