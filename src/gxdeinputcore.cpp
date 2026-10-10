#include "gxdeinputcore.h"
#include "gxdexkbhandler.h"
#include <QDebug>
#include <utility>
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
        return dispatchKeysym(d->xkb.keysymFromKeycode(keycode));

    // xkbcommon 不可用：无法解析按键语义，一律透传（构造 GXDEXkbHandler
    // 时已告警）。中文输入不可用，但按键不会被输入法吞掉
    return false;
}

bool GXDEInputCore::dispatchKeysym(uint32_t keysym)
{
    // 空键位/未映射键解析不出 keysym：透传，消费掉就等于销毁按键
    if (keysym == XKB_KEY_NoSymbol)
        return false;

    // 修饰键自身与 Ctrl/Alt/Super 组合键一律透传：应用需要完整的
    // 修饰键按下状态和 Ctrl+C、Alt+Tab 等快捷键，输入法不参与
    if (isModifierKeysym(keysym) || d->xkb.commandModifierActive())
        return false;

    // 组字导航键：组字中是移动候选高亮、翻页与删字，消费；
    // 无组字时透传，应用才能正常移动光标、翻页、退格
    const bool composing = isComposing();
    switch (keysym) {
        case XKB_KEY_Up:
            if (!composing)
                return false;
            emit keyUp();
            return true;
        case XKB_KEY_Down:
            if (!composing)
                return false;
            emit keyDown();
            return true;
        case XKB_KEY_Page_Up:
            if (!composing)
                return false;
            emit keyPageUp();
            return true;
        case XKB_KEY_Page_Down:
            if (!composing)
                return false;
            emit keyPageDown();
            return true;
        case XKB_KEY_BackSpace:
            if (!composing)
                return false;
            emit keyBackspace();
            return true;
        default:
            break;
    }

    // 主键盘数字1~9：组字中为选词（对应页内索引0~8），
    // 无组字时透传，应用才能正常输入数字
    if (keysym >= XKB_KEY_1 && keysym <= XKB_KEY_9) {
        if (!composing)
            return false;
        emit keyNumber(static_cast<int>(keysym - XKB_KEY_1));
        return true;
    }

    // 其余按键（字母、标点、空格、Enter、Tab、Esc、功能键等）交由拼音
    // 引擎判定是否消费：librime 处理了就是输入法的行为（字母开始/继续
    // 组字、标点转全角上屏、组字中空格选词、Enter 上屏原始输入），
    // 未处理则透传给应用（无组字时的 Enter/Tab/Esc/空格/方向键等）。
    // 引擎未绑定时同样透传
    if (m_keysymProcessor)
        return m_keysymProcessor(keysym);
    return false;
}

bool GXDEInputCore::isComposing() const
{
    return m_composingQuery ? m_composingQuery() : false;
}

bool GXDEInputCore::isModifierKeysym(uint32_t keysym)
{
    switch (keysym) {
        case XKB_KEY_Shift_L:
        case XKB_KEY_Shift_R:
        case XKB_KEY_Control_L:
        case XKB_KEY_Control_R:
        case XKB_KEY_Alt_L:
        case XKB_KEY_Alt_R:
        case XKB_KEY_Meta_L:
        case XKB_KEY_Meta_R:
        case XKB_KEY_Super_L:
        case XKB_KEY_Super_R:
        case XKB_KEY_Hyper_L:
        case XKB_KEY_Hyper_R:
        case XKB_KEY_Caps_Lock:
        case XKB_KEY_Num_Lock:
        case XKB_KEY_Scroll_Lock:
        case XKB_KEY_Mode_switch:
        case XKB_KEY_ISO_Level3_Shift:
            return true;
        default:
            return false;
    }
}

void GXDEInputCore::setKeysymProcessor(KeysymProcessor processor)
{
    m_keysymProcessor = std::move(processor);
}

void GXDEInputCore::setComposingQuery(ComposingQuery query)
{
    m_composingQuery = std::move(query);
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
    // 出站链路：textCommitted 由 GXDEWaylandIM 消费，经
    // zwp_input_method_v2 commit_string + commit 写回合成器
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
