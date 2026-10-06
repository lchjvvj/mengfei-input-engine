#ifndef GXDEXKBHANDLER_H
#define GXDEXKBHANDLER_H

#include <cstdint>

#include <QString>

struct xkb_context;
struct xkb_keymap;
struct xkb_state;

// xkbcommon 键码解析封装（骨架）
// 职责：把合成器传来的 raw evdev keycode 解析成 keysym/可打印文本，
// 兼容不同键盘布局，替代 GXDEInputCore 中的硬编码键码表。
// 完整实现还需：接收 Wayland keyboard 的 modifier 事件调用 updateMask，
// 以及在合成器 keymap 变更时调用 reloadKeymap 热重载布局。
class GXDEXkbHandler
{
public:
    GXDEXkbHandler();
    ~GXDEXkbHandler();

    GXDEXkbHandler(const GXDEXkbHandler &) = delete;
    GXDEXkbHandler &operator=(const GXDEXkbHandler &) = delete;

    // xkbcommon 是否初始化成功；失败时调用方应回退到硬编码键码表
    bool isValid() const;

    // 按 RMLVO 规则加载 keymap，参数为空表示使用系统默认；
    // layout 例如 "us"、"cn"
    bool reloadKeymap(const QString &rules, const QString &model,
                      const QString &layout, const QString &variant,
                      const QString &options);

    // evdev raw keycode -> keysym（XKB_KEY_*），失败返回 XKB_KEY_NoSymbol
    uint32_t keysymFromKeycode(uint32_t evdevKeycode) const;

    // keysym -> UTF-8 文本；不可打印（功能键/控制键）返回空串
    static QString textFromKeysym(uint32_t keysym);

    // TODO: 收到 Wayland wl_keyboard.modifiers 事件后调用，
    // 同步 shift/ctrl/capslock 等状态，保证 keysym 解析结果正确
    void updateMask(uint32_t depressedMods, uint32_t latchedMods,
                    uint32_t lockedMods, uint32_t group);

private:
    xkb_context *m_context = nullptr;
    xkb_keymap *m_keymap = nullptr;
    xkb_state *m_state = nullptr;
};

#endif // GXDEXKBHANDLER_H
