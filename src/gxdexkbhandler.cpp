#include "gxdexkbhandler.h"

#include <QDebug>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-names.h>

// xkbcommon 约定：xkb keycode = evdev keycode + 8
static const uint32_t kEvdevToXkbOffset = 8;

GXDEXkbHandler::GXDEXkbHandler()
{
    m_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!m_context) {
        qWarning() << "[GXDEXkb] 创建 xkb_context 失败，按键将直接透传给应用";
        return;
    }
    // 默认按系统配置加载 keymap（RMLVO 全部留空即取默认值）
    reloadKeymap(QString(), QString(), QString(), QString(), QString());
}

GXDEXkbHandler::~GXDEXkbHandler()
{
    if (m_state) xkb_state_unref(m_state);
    if (m_keymap) xkb_keymap_unref(m_keymap);
    if (m_context) xkb_context_unref(m_context);
}

bool GXDEXkbHandler::isValid() const
{
    return m_context && m_keymap && m_state;
}

bool GXDEXkbHandler::reloadKeymap(const QString &rules, const QString &model,
                                  const QString &layout, const QString &variant,
                                  const QString &options)
{
    if (!m_context)
        return false;

    // 先转存 UTF-8，避免 qPrintable 临时对象悬空
    const QByteArray rulesUtf8 = rules.toUtf8();
    const QByteArray modelUtf8 = model.toUtf8();
    const QByteArray layoutUtf8 = layout.toUtf8();
    const QByteArray variantUtf8 = variant.toUtf8();
    const QByteArray optionsUtf8 = options.toUtf8();

    xkb_rule_names names;
    names.rules = rules.isEmpty() ? nullptr : rulesUtf8.constData();
    names.model = model.isEmpty() ? nullptr : modelUtf8.constData();
    names.layout = layout.isEmpty() ? nullptr : layoutUtf8.constData();
    names.variant = variant.isEmpty() ? nullptr : variantUtf8.constData();
    names.options = options.isEmpty() ? nullptr : optionsUtf8.constData();

    xkb_keymap *keymap = xkb_keymap_new_from_names(m_context, &names,
                                                   XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        qWarning() << "[GXDEXkb] 加载 keymap 失败, layout:" << layout;
        return false;
    }

    xkb_state *state = xkb_state_new(keymap);
    if (!state) {
        xkb_keymap_unref(keymap);
        qWarning() << "[GXDEXkb] 创建 xkb_state 失败";
        return false;
    }

    // 整体替换旧 keymap/state，支持布局热切换
    if (m_state) xkb_state_unref(m_state);
    if (m_keymap) xkb_keymap_unref(m_keymap);
    m_keymap = keymap;
    m_state = state;
    qDebug() << "[GXDEXkb] keymap 已加载, layout:"
             << (layout.isEmpty() ? QStringLiteral("系统默认") : layout);
    return true;
}

uint32_t GXDEXkbHandler::keysymFromKeycode(uint32_t evdevKeycode) const
{
    if (!isValid())
        return XKB_KEY_NoSymbol;

    const xkb_keycode_t keycode = static_cast<xkb_keycode_t>(evdevKeycode) + kEvdevToXkbOffset;
    return static_cast<uint32_t>(xkb_state_key_get_one_sym(m_state, keycode));
}

bool GXDEXkbHandler::loadKeymapFromString(const QByteArray &keymapString)
{
    if (!m_context || keymapString.isEmpty())
        return false;

    // constData 保证 NUL 结尾：keymap 事件带来的字符串本身含结尾 NUL
    xkb_keymap *keymap = xkb_keymap_new_from_string(m_context, keymapString.constData(),
                                                    XKB_TEXT_FORMAT_V1,
                                                    XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        qWarning() << "[GXDEXkb] 解析合成器 keymap 失败";
        return false;
    }

    xkb_state *state = xkb_state_new(keymap);
    if (!state) {
        xkb_keymap_unref(keymap);
        qWarning() << "[GXDEXkb] 创建 xkb_state 失败";
        return false;
    }

    // 整体替换旧 keymap/state（与 reloadKeymap 相同的热切换语义）
    if (m_state) xkb_state_unref(m_state);
    if (m_keymap) xkb_keymap_unref(m_keymap);
    m_keymap = keymap;
    m_state = state;
    qDebug() << "[GXDEXkb] 合成器 keymap 已加载";
    return true;
}

bool GXDEXkbHandler::keyRepeats(uint32_t evdevKeycode) const
{
    if (!isValid())
        return false;

    const xkb_keycode_t keycode = static_cast<xkb_keycode_t>(evdevKeycode) + kEvdevToXkbOffset;
    return xkb_keymap_key_repeats(m_keymap, keycode) != 0;
}

bool GXDEXkbHandler::commandModifierActive() const
{
    if (!isValid())
        return false;

    // 修饰键名按 keymap 的标准命名查询：Ctrl / Alt(Mod1) / Super(Mod4)
    static const char *const kCommandMods[] = {
        XKB_MOD_NAME_CTRL, XKB_MOD_NAME_ALT, XKB_MOD_NAME_LOGO,
    };
    for (const char *name : kCommandMods) {
        if (xkb_state_mod_name_is_active(m_state, name, XKB_STATE_MODS_EFFECTIVE) > 0)
            return true;
    }
    return false;
}

void GXDEXkbHandler::updateMask(uint32_t depressedMods, uint32_t latchedMods,
                                uint32_t lockedMods, uint32_t group)
{
    if (!isValid())
        return;
    // 参数直接来自 Wayland 键盘抓取的 modifiers 事件（GXDEWaylandIM 转发）
    xkb_state_update_mask(m_state, depressedMods, latchedMods, lockedMods,
                          0, 0, group);
}
