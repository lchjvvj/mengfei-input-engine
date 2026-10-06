#ifndef GXDEINPUTCORE_H
#define GXDEINPUTCORE_H

#include <QObject>
#include <QString>

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

    // 按键事件入口，从合成器接收按键
    bool handleKeyEvent(uint32_t keycode, bool pressed);

    // 提交文本给应用
    void commitString(const QString &str);
    void setPreedit(const QString &text, int cursor);
    void setCandidates(const QStringList &words);

signals:
    // 通知UI更新候选栏
    void preeditChanged(const QString &text, int cursor);
    void candidateListChanged(const QStringList &list, int sel);
    void textCommitted(const QString &text);

private:
    GXDEInputContext m_ctx;
    class Private;
    Private *d;
};

#endif // GXDEINPUTCORE_H
