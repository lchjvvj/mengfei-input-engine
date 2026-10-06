#ifndef GXDEINPUTCORE_H
#define GXDEINPUTCORE_H

#include <cstdint>

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

    // 激活/关闭输入上下文（由gxde-wlcom通知）
    void activateContext();
    void deactivateContext();

    // 中英文输入模式（设置面板"中文输入"开关与 Shift 单独敲击共同驱动）：
    // 英文模式下 handleKeyEvent 不消费任何按键，合成器直接透传给应用，
    // 字母不经过输入法直接上屏
    void setChineseMode(bool chinese);
    bool chineseMode() const;

    // 按键事件入口，从合成器接收按键
    bool handleKeyEvent(uint32_t keycode, bool pressed);

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
    // 按键分发信号：普通字符键、方向键、翻页、数字选词、退格
    void keyRawEvent(uint32_t keycode);
    void keyCharacter(const QString &text); // xkbcommon 解析出的可打印文本
    void keyUp();
    void keyDown();
    void keyPageUp();
    void keyPageDown();
    void keyNumber(int num);
    void keyBackspace();

private:
    // xkbcommon 解析出的 keysym 分发到对应信号
    bool dispatchKeysym(uint32_t keysym, uint32_t rawKeycode);
    // 判断 raw keycode 是否为 Shift 键（xkb 解析失败时回退 evdev 硬编码）
    bool isShiftKey(uint32_t keycode) const;

    GXDEInputContext m_ctx;
    bool m_chineseMode = true;   // 默认中文输入
    bool m_shiftHeld = false;    // Shift 当前处于按下状态
    bool m_shiftAlone = false;   // Shift 按住期间未敲过其他键（单独敲击）
    uint32_t m_shiftKeycode = 0; // 按下的 Shift 键码，用于识别同键自动重复
    class Private;
    Private *d;
};

#endif // GXDEINPUTCORE_H
