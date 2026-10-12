#ifndef GXDEINPUTCORE_H
#define GXDEINPUTCORE_H

#include <cstdint>
#include <functional>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

// Wayland text-input-v3 状态封装
struct GXDEInputContext
{
    bool active = false;
    QString preeditText;       // 预编辑文字
    int preeditCursor = 0;
    QString commitText;         // 确认提交的文字
    QStringList candidates;     // 候选词列表
    int selectedIndex = 0;
};

class GXDEInputCore : public QObject
{
    Q_OBJECT
public:
    explicit GXDEInputCore(QObject *parent = nullptr);
    ~GXDEInputCore() override;

    // 激活/关闭输入上下文（由合成器 zwp_input_method_v2 的
    // activate/deactivate 事件驱动，见 GXDEWaylandIM）
    void activateContext();
    void deactivateContext();

    // 中英文输入模式（设置面板"中文输入"开关与 Shift 单独敲击共同驱动）：
    // 英文模式下 handleKeyEvent 不消费任何按键，合成器直接透传给应用，
    // 字母不经过输入法直接上屏
    void setChineseMode(bool chinese);
    bool chineseMode() const;

    // 按键事件入口，从合成器接收按键。返回 true 表示输入法已消费该按键，
    // 返回 false 表示调用方（GXDEWaylandIM）应把按键透传给焦点应用：
    //  - 上下文未激活、释放事件、英文模式：一律透传
    //  - 中文模式：修饰键自身与 Ctrl/Alt/Super 组合键透传（应用快捷键）；
    //    方向键/翻页/退格/数字仅在组字中消费（选词与删字）；其余按键
    //    （字母/标点/空格/Enter/Tab/Esc/功能键等）由拼音引擎判定，
    //    Rime 不消费的一律透传，不允许出现"按键被输入法吞掉"
    bool handleKeyEvent(uint32_t keycode, bool pressed);

    // 中文模式下的按键判定回调（main.cpp 绑定到 GXDEPinyinEngine）：
    //  - KeysymProcessor：把 keysym 交给拼音引擎（librime），返回是否已消费。
    //    字母组字、标点转全角上屏、组字中空格选词/Enter 上屏原始输入被消费，
    //    无组字时的 Enter/Tab/Esc/空格等返回未消费，按键透传给焦点应用
    //  - ComposingQuery：是否存在未完成的组字。方向键/翻页/退格/数字仅在
    //    组字中是选词与删字操作，无组字时必须透传，否则应用收不到按键
    // 未绑定时按键一律透传：输入法宁可不做处理，也不能吞掉按键
    using KeysymProcessor = std::function<bool(uint32_t keysym)>;
    using ComposingQuery = std::function<bool()>;
    void setKeysymProcessor(KeysymProcessor processor);
    void setComposingQuery(ComposingQuery query);

    // Wayland 输入法客户端（GXDEWaylandIM）对接：同步合成器下发的
    // 键盘布局与修饰键状态，保证 keysym 解析和合成器/应用一致；
    // keyRepeats 供 Wayland 端按 xkb 语义实现按键自动重复
    bool loadKeymap(const QByteArray &keymapString);
    void updateModifiers(uint32_t depressedMods, uint32_t latchedMods,
                         uint32_t lockedMods, uint32_t group);
    bool keyRepeats(uint32_t keycode) const;

    // 提交文本给应用
    void commitString(const QString &str);
    void setPreedit(const QString &text, int cursor);
    void setCandidates(const QStringList &words);

signals:
    // 中英文模式切换
    void chineseModeChanged(bool chinese);
    // 通知UI更新候选栏
    void preeditChanged(const QString &text, int cursor);
    void candidateListChanged(const QStringList &list, int sel);
    void textCommitted(const QString &text);
    // 按键分发信号：方向键、翻页、数字选词、退格。仅在组字中发出
    //（无组字时这些按键透传给应用）；字符与标点按键不走信号，
    // 由 KeysymProcessor 直接交给拼音引擎判定是否消费
    void keyUp();
    void keyDown();
    void keyPageUp();
    void keyPageDown();
    void keyNumber(int num);
    void keyBackspace();

private:
    // xkbcommon 解析出的 keysym 分发：返回 true 表示按键已被输入法消费，
    // 返回 false 表示应透传给焦点应用
    bool dispatchKeysym(uint32_t keysym);
    // 是否存在未完成的组字（未绑定 ComposingQuery 时为 false）
    bool isComposing() const;
    // 判断 raw keycode 是否为 Shift 键（xkb 解析失败时回退 evdev 硬编码）
    bool isShiftKey(uint32_t keycode) const;
    // 判断 keysym 是否为修饰键自身（Shift/Ctrl/Alt/Super/CapsLock 等）：
    // 修饰键的按下与释放必须送达应用，否则应用侧组合键状态丢失
    static bool isModifierKeysym(uint32_t keysym);

    GXDEInputContext m_ctx;
    bool m_chineseMode = true;   // 默认中文输入
    bool m_shiftHeld = false;    // Shift 当前处于按下状态
    bool m_shiftAlone = false;   // Shift 按住期间未敲过其他键（单独敲击）
    uint32_t m_shiftKeycode = 0; // 按下的 Shift 键码，用于识别同键自动重复
    KeysymProcessor m_keysymProcessor;  // 按键消费判定（绑定拼音引擎）
    ComposingQuery m_composingQuery;    // 组字状态查询（绑定拼音引擎）
    class Private;
    Private *d;
};

#endif // GXDEINPUTCORE_H
