#include "gxdeinputcore.h"
#include <QDebug>

// 前置声明 Wayland 协议类型，真实项目由 wayland-scanner 从
// protocol/zwp-text-input-v3.xml 生成完整定义
struct zwp_text_input_v3;
struct wl_display;

class GXDEInputCore::Private
{
public:
    // 简化：真实项目需要wayland协议生成的struct与回调
    zwp_text_input_v3 *textInput = nullptr;
    wl_display *display = nullptr;
};

GXDEInputCore::GXDEInputCore(QObject *parent)
    : QObject(parent), d(new Private)
{
}

GXDEInputCore::~GXDEInputCore()
{
    delete d;
}

void GXDEInputCore::activateContext()
{
    m_ctx.active = true;
    qDebug() << "[GXDEInput] Input context activated";
}

void GXDEInputCore::deactivateContext()
{
    m_ctx.active = false;
    m_ctx.preeditText.clear();
    m_ctx.candidates.clear();
    emit preeditChanged("", 0);
    emit candidateListChanged({}, 0);
    qDebug() << "[GXDEInput] Input context deactivated";
}

bool GXDEInputCore::handleKeyEvent(uint32_t keycode, bool pressed)
{
    if (!m_ctx.active || !pressed)
        return false;

    // 这里是简化映射，真实环境需要使用libinput的keysym转换
    switch(keycode)
    {
        case 106: // Down
            emit keyDown();
            return true;
        case 103: // Up
            emit keyUp();
            return true;
        case 104: // PageUp
            emit keyPageUp();
            return true;
        case 109: // PageDown
            emit keyPageDown();
            return true;
        case 2: case 3: case 4: case 5: case 6:
        case 7: case 8: case 9: case 10: // 主键盘1~9
            emit keyNumber(keycode - 2);
            return true;
        default:
            // 普通字符，传给拼音引擎
            emit keyRawEvent(keycode);
            return true;
    }
}

void GXDEInputCore::commitString(const QString &str)
{
    if (str.isEmpty()) return;
    m_ctx.commitText = str;
    emit textCommitted(str);
    qDebug() << "[GXDEInput] Commit:" << str;
    // 实际：调用 zwp_text_input_v3_commit_string(d->textInput, str.toUtf8().constData());
}

void GXDEInputCore::setPreedit(const QString &text, int cursor)
{
    m_ctx.preeditText = text;
    m_ctx.preeditCursor = cursor;
    emit preeditChanged(text, cursor);
}

void GXDEInputCore::setCandidates(const QStringList &words)
{
    m_ctx.candidates = words;
    emit candidateListChanged(words, m_ctx.selectedIndex);
}
