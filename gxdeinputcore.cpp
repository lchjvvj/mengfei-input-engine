#include "gxdeinputcore.h"
#include <QDebug>

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
    // 只转发字符按键，修饰键留给合成器处理
    emit keyRawEvent(keycode);
    return true;
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
