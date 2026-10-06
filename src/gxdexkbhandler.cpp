#include "gxdexkbhandler.h"

#include <QDebug>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

// xkbcommon 约定：xkb keycode = evdev keycode + 8
static const uint32_t kEvdevToXkbOffset = 8;

GXDEXkbHandler::GXDEXkbHandler()
{
    m_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!m_context) {
        qWarning() << "[GXDEXkb] 创建 xkb_context 失败，回退硬编码键码表";
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

QString GXDEXkbHandler::textFromKeysym(uint32_t keysym)
{
    if (keysym == XKB_KEY_NoSymbol)
        return QString();

    char buf[64] = {0};
    const int len = xkb_keysym_to_utf8(static_cast<xkb_keysym_t>(keysym), buf, sizeof(buf));
    if (len <= 0)
        return QString();

    // 不同 xkbcommon 版本对返回值的定义不同（是否含结尾 NUL），
    // 统一按 NUL 截断取串（buf 已零初始化，xkb 保证写入 NUL 结尾）
    const QString text = QString::fromUtf8(buf);
    // 过滤控制字符（Enter/Tab 等），只保留可打印文本
    if (text.isEmpty() || text.at(0).category() == QChar::Other_Control)
        return QString();
    return text;
}

void GXDEXkbHandler::updateMask(uint32_t depressedMods, uint32_t latchedMods,
                                uint32_t lockedMods, uint32_t group)
{
    if (!isValid())
        return;
    // TODO: 布局参数目前直接透传 group，接入 Wayland 后按协议事件填充
    xkb_state_update_mask(m_state, depressedMods, latchedMods, lockedMods,
                          0, 0, group);
}
