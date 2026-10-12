#include <QGuiApplication>
#include <QDebug>

#include "gxdedbusadapter.h"
#include "gxdeinputcore.h"
#include "gxdepinyinengine.h"
#include "gxdesettings.h"
#include "gxdewaylandim.h"
#include "ui/CandidateBar.hpp"
#include "ui/SettingsPanel.hpp"

int main(int argc, char *argv[])
{
    // 设置面板仍是 QML 窗口，必须使用QGuiApplication，不能QCoreApplication！
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("mengfei-input-engine");
    // 产品名：梦飞输入法（窗口标题等用户可见场景的默认应用名）
    QGuiApplication::setApplicationDisplayName(QStringLiteral("梦飞输入法"));
    // 引擎是后台常驻服务（systemd user 单元）：关闭设置面板窗口
    // 是"最后一个窗口关闭"，不能触发进程退出
    QGuiApplication::setQuitOnLastWindowClosed(false);

    // 全局设置：中英文切换、候选栏样式、主题；设置面板与候选栏共用
    GXDESettings settings;
    GXDEInputCore core;
    GXDEPinyinEngine pinyin;
    // 候选栏渲染器：不创建窗口，出帧交给 GXDEWaylandIM 经
    // zwp_input_popup_surface_v2 显示
    CandidateBar ui(&settings);
    SettingsPanel settingsPanel(&settings);

    // DBus 辅助通道：主输入链路（激活/按键/候选窗/文本回写）全部走
    // zwp_input_method_v2（见下方 GXDEWaylandIM），DBus 仅保留供
    // GXDE 控制中心打开设置面板的 ShowSettings 管理入口；
    // 注册结果同时充当单实例守护
    GXDEDBusAdapter dbusAdapter(&settingsPanel);
    if (!dbusAdapter.registerService()) {
        qCritical() << "DBus 服务注册失败，退出（请检查是否已有 mengfei-input-engine 实例在运行）";
        return -1;
    }

    // 标准 Wayland 输入法通道：zwp_input_method_v2 客户端直连 gxde-wlcom。
    // activate/deactivate 驱动输入上下文，grab_keyboard 的按键进入
    // core.handleKeyEvent，未消费按键经 zwp_virtual_keyboard_v1 转发给
    // 焦点应用，preedit/上屏文本经 commit(serial) 写回合成器；候选窗
    // 帧经 zwp_input_popup_surface_v2 提交，由合成器放置在焦点文本
    // 输入区域附近；连接失败或断开时每 5 秒自动重连
    GXDEWaylandIM waylandIM(&core, &pinyin, &ui);
    waylandIM.start();

    // 信号槽绑定：拼音引擎 <-> 候选栏
    QObject::connect(&pinyin, &GXDEPinyinEngine::preeditUpdate, &ui, &CandidateBar::updatePreedit);
    QObject::connect(&pinyin, &GXDEPinyinEngine::candidateUpdate, &ui, [&](const QStringList &cands, int selIdx){
        ui.updateCandidates(cands, selIdx);
        if(!cands.isEmpty()) ui.show();
        else ui.hide();
    });
    QObject::connect(&pinyin, &GXDEPinyinEngine::commitWord, &core, &GXDEInputCore::commitString);
    // 页码更新：候选栏显示 1/2 形式的页码
    QObject::connect(&pinyin, &GXDEPinyinEngine::pageInfoUpdate, &ui, &CandidateBar::updatePageInfo);

    // 信号绑定：方向键、翻页、数字选词（仅组字中发出，无组字时按键透传给应用）
    QObject::connect(&core, &GXDEInputCore::keyUp, &pinyin, &GXDEPinyinEngine::selectPrev);
    QObject::connect(&core, &GXDEInputCore::keyDown, &pinyin, &GXDEPinyinEngine::selectNext);
    QObject::connect(&core, &GXDEInputCore::keyPageUp, &pinyin, &GXDEPinyinEngine::pageUp);
    QObject::connect(&core, &GXDEInputCore::keyPageDown, &pinyin, &GXDEPinyinEngine::pageDown);
    QObject::connect(&core, &GXDEInputCore::keyNumber, &pinyin, &GXDEPinyinEngine::selectCandidate);
    QObject::connect(&core, &GXDEInputCore::keyBackspace, &pinyin, &GXDEPinyinEngine::backspace);

    // 中文模式的按键判定：字母、标点、空格、Enter、Tab、Esc、功能键等
    // 交给 librime，由 Rime 决定是否消费（字母组字、标点转全角上屏、
    // 组字中空格选词/Enter 上屏原始输入）；Rime 未消费的按键由
    // GXDEWaylandIM 经虚拟键盘透传给焦点应用，不再被输入法吞掉。
    // 组字状态用于判定方向键/翻页/退格/数字是选词操作还是应透传
    core.setKeysymProcessor([&pinyin](uint32_t keysym) {
        return pinyin.processKey(static_cast<int>(keysym));
    });
    core.setComposingQuery([&pinyin] { return pinyin.isComposing(); });

    // 中英文切换：设置面板开关与 Shift 单独敲击双向同步。
    // 英文模式下 handleKeyEvent 一律不消费，合成器把按键直接透传给应用，
    // 字母不经输入法直接上屏；切到英文时清空正在组字的拼音
    //（触发空候选，候选栏自动隐藏）
    core.setChineseMode(settings.chineseEnabled());
    QObject::connect(&settings, &GXDESettings::chineseEnabledChanged,
                     &core, &GXDEInputCore::setChineseMode);
    // Shift 敲击切换的模式变化回写设置：持久化到 ini 并同步面板开关；
    // setChineseMode/setChineseEnabled 值相等时均直接返回，不会循环触发
    QObject::connect(&core, &GXDEInputCore::chineseModeChanged,
                     &settings, &GXDESettings::setChineseEnabled);
    QObject::connect(&settings, &GXDESettings::chineseEnabledChanged,
                     &pinyin, [&pinyin](bool enabled){
        if (!enabled)
            pinyin.clear();
    });

    // 输入上下文激活时显示UI，失活隐藏
    QObject::connect(&core, &GXDEInputCore::preeditChanged, &ui, &CandidateBar::updatePreedit);
    QObject::connect(&core, &GXDEInputCore::candidateListChanged, &ui, &CandidateBar::updateCandidates);
    QObject::connect(&core, &GXDEInputCore::textCommitted, &ui, &CandidateBar::hide);

    // 输入上下文激活不再本地模拟：由合成器经 zwp_input_method_v2 的
    // activate/deactivate 事件驱动（GXDEWaylandIM）。候选窗定位也不再
    // 需要坐标推送：zwp_input_popup_surface_v2 由合成器贴着 text-input-v3
    // set_cursor_rectangle 上报的文本输入矩形放置，并自动做屏幕边界避让

    qDebug() << "Mengfei Input Engine (梦飞输入法) with CandidateBar UI running...";
    return app.exec();
}
