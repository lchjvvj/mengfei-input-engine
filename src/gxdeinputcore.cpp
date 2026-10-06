#include "gxdeinputcore.h"
#include "gxdexkbhandler.h"
#include <QDebug>
#include <xkbcommon/xkbcommon-keysyms.h>

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
    // xkbcommon 键码解析：raw keycode -> keysym，兼容不同键盘布局
    GXDEXkbHandler xkb;
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

void GXDEInputCore::setChineseMode(bool chinese)
{
    if (m_chineseMode == chinese)
        return;
    m_chineseMode = chinese;
    emit chineseModeChanged(chinese);
    qDebug() << "[GXDEInput] Input mode:" << (chinese ? "Chinese" : "English");
}

bool GXDEInputCore::chineseMode() const
{
    return m_chineseMode;
}

bool GXDEInputCore::handleKeyEvent(uint32_t keycode, bool pressed)
{
    if (!m_ctx.active || !pressed)
        return false;

    // 英文模式：不消费按键，合成器按普通键盘输入直接透传给应用
    if (!m_chineseMode)
        return false;

    // 优先使用 xkbcommon 解析 keysym（兼容不同键盘布局）
    if (d->xkb.isValid())
        return dispatchKeysym(d->xkb.keysymFromKeycode(keycode), keycode);

    // 回退：旧的硬编码键码表，仅在 xkbcommon 初始化失败时使用
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

bool GXDEInputCore::dispatchKeysym(uint32_t keysym, uint32_t rawKeycode)
{
    switch (keysym) {
        case XKB_KEY_Up:
            emit keyUp();
            return true;
        case XKB_KEY_Down:
            emit keyDown();
            return true;
        case XKB_KEY_Page_Up:
            emit keyPageUp();
            return true;
        case XKB_KEY_Page_Down:
            emit keyPageDown();
            return true;
        case XKB_KEY_BackSpace:
            emit keyBackspace();
            return true;
        default:
            break;
    }

    // 主键盘数字1~9：选词，对应页内索引0~8
    if (keysym >= XKB_KEY_1 && keysym <= XKB_KEY_9) {
        emit keyNumber(static_cast<int>(keysym - XKB_KEY_1));
        return true;
    }

    // 可打印字符：xkbcommon 转成文本后交给拼音引擎
    const QString text = GXDEXkbHandler::textFromKeysym(keysym);
    if (!text.isEmpty()) {
        emit keyCharacter(text);
        return true;
    }

    // 其余按键（功能键等）保持与旧逻辑一致，原样透传 raw keycode
    emit keyRawEvent(rawKeycode);
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
