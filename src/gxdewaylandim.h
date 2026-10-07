#ifndef GXDEWAYLANDIM_H
#define GXDEWAYLANDIM_H

#include <QObject>

class GXDEInputCore;
class GXDEPinyinEngine;

// 标准 zwp_input_method_v2 输入法客户端：直连 gxde-wlcom 合成器。
// 合成器（wlroots 系）已实现 input-method-unstable-v2 与
// virtual-keyboard-unstable-v1，本类走标准协议对接，替代合成器侧
// 没有客户端代码的 DBus 通道（DBus 通道保留，供控制中心等调用）：
//  - wl_display_connect 建立独立 Wayland 连接，QSocketNotifier 接入
//    Qt 事件循环；连接失败/断开后每 5 秒自动重连（合成器可能未启动或重启中）
//  - activate/deactivate 事件驱动 GXDEInputCore 输入上下文的激活与关闭
//  - grab_keyboard 取得独占按键抓取后交给 GXDEInputCore::handleKeyEvent；
//    未消费的按键经 zwp_virtual_keyboard_v1 转发，合成器识别其来自输入法
//    并直通焦点应用；合成器对路由给输入法的按键停止了自动重复，
//    由本类按 repeat_info（rate/delay）补齐按键重复
//  - 修饰键与 keymap 同步到 GXDEXkbHandler，并经虚拟键盘转发给应用
//  - preedit/上屏文本经 set_preedit_string/commit_string + commit(serial)
//    写回合成器；serial 按协议约定等于已收到的 done 事件计数
class GXDEWaylandIM : public QObject
{
    Q_OBJECT
public:
    explicit GXDEWaylandIM(GXDEInputCore *core, GXDEPinyinEngine *pinyin,
                           QObject *parent = nullptr);
    ~GXDEWaylandIM() override;

    // 启动合成器连接：首次立即尝试，失败转定时重试（不阻塞主流程）
    void start();

private:
    class Private;
    Private *d;
};

#endif // GXDEWAYLANDIM_H
