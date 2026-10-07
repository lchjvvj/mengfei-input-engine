#include "gxdeinputcore.h"
#include "gxdexkbhandler.h"
#include <QDebug>
#include <xkbcommon/xkbcommon-keysyms.h>

class GXDEInputCore::Private
{
public:
    // xkbcommon 键码解析：raw keycode -> keysym，兼容不同键盘布局。
    // Wayland 协议对接由 GXDEWaylandIM 完成，经 loadKeymap/updateModifiers
    // 把合成器的键盘布局与修饰键状态同步到这里
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
    // 光标矩形随上下文清空（text-input-v3：矩形仅在两次 commit 的
    // enable/disable 之间有效，失活后回到空矩形初始状态）
    m_ctx.cursorX = 0;
    m_ctx.cursorY = 0;
    m_ctx.cursorWidth = 0;
    m_ctx.cursorHeight = 0;
    // 失活时丢弃未完成的 Shift 敲击跟踪，避免下次激活误触发切换
    m_shiftHeld = false;
    m_shiftAlone = false;
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
    if (!m_ctx.active)
        return false;

    // Shift 单独敲击切换中英文：按下时标记"单独敲击"，按住期间敲过
    // 任何其他键（含另一个 Shift）视为组合键；释放时若仍是单独敲击，
    // 切换中英文模式。Shift 本身按下/释放均不消费，应用侧修饰键状态保持一致
    const bool shiftKey = isShiftKey(keycode);
    if (pressed) {
        if (shiftKey) {
            if (m_shiftHeld && keycode != m_shiftKeycode) {
                m_shiftAlone = false;   // 双 Shift 同按：视为组合，不触发切换
            } else {
                // 首次按下或同键自动重复：保持"单独敲击"标记
                if (!m_shiftHeld)
                    m_shiftAlone = true;
                m_shiftHeld = true;
                m_shiftKeycode = keycode;
            }
        } else if (m_shiftHeld) {
            m_shiftAlone = false;       // Shift+其他键：组合，不触发切换
        }
    } else if (shiftKey && m_shiftHeld) {
        const bool alone = m_shiftAlone;
        m_shiftHeld = false;
        m_shiftAlone = false;
        if (alone)
            setChineseMode(!m_chineseMode);
    }

    // 释放事件一律不消费（Shift 切换已在上方触发，按键仍透传给应用）
    if (!pressed)
        return false;

    // 英文模式：不消费按键，合成器按普通键盘输入直接透传给应用，
    // 字母不经输入法直接上屏
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

bool GXDEInputCore::isShiftKey(uint32_t keycode) const
{
    // 优先 xkbcommon 解析（Shift 自身 keysym 不受修饰键状态影响，解析可靠）
    if (d->xkb.isValid()) {
        const uint32_t keysym = d->xkb.keysymFromKeycode(keycode);
        return keysym == XKB_KEY_Shift_L || keysym == XKB_KEY_Shift_R;
    }
    // 回退：evdev 硬编码键码，左Shift=42，右Shift=54
    return keycode == 42 || keycode == 54;
}

bool GXDEInputCore::loadKeymap(const QByteArray &keymapString)
{
    return d->xkb.loadKeymapFromString(keymapString);
}

void GXDEInputCore::updateModifiers(uint32_t depressedMods, uint32_t latchedMods,
                                    uint32_t lockedMods, uint32_t group)
{
    d->xkb.updateMask(depressedMods, latchedMods, lockedMods, group);
}

bool GXDEInputCore::keyRepeats(uint32_t keycode) const
{
    return d->xkb.keyRepeats(keycode);
}

void GXDEInputCore::commitString(const QString &str)
{
    if (str.isEmpty()) return;
    m_ctx.commitText = str;
    emit textCommitted(str);
    qDebug() << "[GXDEInput] Commit:" << str;
    // 出站链路：textCommitted 由 GXDEWaylandIM（zwp_input_method_v2
    // commit_string + commit）与 DBus 适配器（CommitText 信号）分别消费
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

void GXDEInputCore::setCursorRect(int x, int y, int width, int height)
{
    m_ctx.cursorX = x;
    m_ctx.cursorY = y;
    m_ctx.cursorWidth = width;
    m_ctx.cursorHeight = height;
    emit cursorRectChanged(x, y, width, height);
}
