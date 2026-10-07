#ifndef GXDEXKBHANDLER_H
#define GXDEXKBHANDLER_H

#include <cstdint>

#include <QByteArray>
#include <QString>

struct xkb_context;
struct xkb_keymap;
struct xkb_state;

// xkbcommon 键码解析封装
// 职责：把合成器传来的 raw evdev keycode 解析成 keysym/可打印文本，
// 兼容不同键盘布局，替代 GXDEInputCore 中的硬编码键码表。
// Wayland 接入后由 GXDEWaylandIM 调用：keymap 事件走 loadKeymapFromString
// 同步合成器布局，modifiers 事件走 updateMask 同步修饰键状态。
class GXDEXkbHandler
{
public:
    GXDEXkbHandler();
    ~GXDEXkbHandler();

    GXDEXkbHandler(const GXDEXkbHandler &) = delete;
    GXDEXkbHandler &operator=(const GXDEXkbHandler &) = delete;

    // xkbcommon 是否初始化成功；失败时无法解析 keysym，调用方应把按键
    // 原样透传给应用（消费掉解析不了的按键等于销毁按键）
    bool isValid() const;

    // 按 RMLVO 规则加载 keymap，参数为空表示使用系统默认；
    // layout 例如 "us"、"cn"
    bool reloadKeymap(const QString &rules, const QString &model,
                      const QString &layout, const QString &variant,
                      const QString &options);

    // 从合成器下发的 keymap 字符串（Wayland keymap 事件的完整内容，
    // 含结尾 NUL）直接加载，替换 RMLVO 默认布局，
    // 保证 keysym 解析与合成器/应用的键盘布局一致
    bool loadKeymapFromString(const QByteArray &keymapString);

    // evdev raw keycode -> keysym（XKB_KEY_*），失败返回 XKB_KEY_NoSymbol
    uint32_t keysymFromKeycode(uint32_t evdevKeycode) const;

    // 按键是否参与自动重复（xkb repeat 语义，与合成器 wl_keyboard 行为一致），
    // 供 Wayland 输入法端实现按键重复
    bool keyRepeats(uint32_t evdevKeycode) const;

    // 命令修饰键（Ctrl/Alt/Super）是否处于按下状态：这类组合键是应用
    // 快捷键（Ctrl+C、Alt+Tab 等），输入法必须原样透传，不参与组字。
    // Shift/CapsLock/NumLock 不计入（大小写与中英文切换需要它们）；
    // AltGr 通常是 Mod5，同样不计入（它用于输入第三层字符）
    bool commandModifierActive() const;

    // 同步修饰键状态：收到 Wayland 键盘抓取的 modifiers 事件后
    // 由 GXDEWaylandIM 经 GXDEInputCore::updateModifiers 调用，
    // 保证 shift/ctrl/capslock 等组合键的 keysym 解析正确
    void updateMask(uint32_t depressedMods, uint32_t latchedMods,
                    uint32_t lockedMods, uint32_t group);

private:
    xkb_context *m_context = nullptr;
    xkb_keymap *m_keymap = nullptr;
    xkb_state *m_state = nullptr;
};

#endif // GXDEXKBHANDLER_H
