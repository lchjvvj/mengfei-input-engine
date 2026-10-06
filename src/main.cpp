#include <QGuiApplication>
#include <QDebug>

#include "gxdedbusadapter.h"
#include "gxdeinputcore.h"
#include "gxdepinyinengine.h"
#include "gxdesettings.h"
#include "ui/CandidateBar.hpp"
#include "ui/SettingsPanel.hpp"

int main(int argc, char *argv[])
{
    // QML窗口必须使用QGuiApplication，不能QCoreApplication！
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("gxde-input-engine");
    // 引擎是后台常驻服务（systemd user 单元）：关闭设置面板窗口
    // 是"最后一个窗口关闭"，不能触发进程退出
    QGuiApplication::setQuitOnLastWindowClosed(false);

    // 全局设置：中英文切换、候选栏样式、主题；设置面板与候选栏共用
    GXDESettings settings;
    GXDEInputCore core;
    GXDEPinyinEngine pinyin;
    CandidateBar ui(&settings);
    SettingsPanel settingsPanel(&settings);

    // DBus双向通信：gxde-wlcom 方法调用驱动引擎（激活/按键/光标坐标/打开设置面板），
    // 引擎信号回传 preedit/候选/上屏文本给合成器
    GXDEDBusAdapter dbusAdapter(&core, &pinyin, &ui, &settingsPanel);
    if (!dbusAdapter.registerService()) {
        qCritical() << "DBus 服务注册失败，退出（请检查是否已有 gxde-input-engine 实例在运行）";
        return -1;
    }

    // 信号槽绑定：拼音引擎 <-> UI
    QObject::connect(&pinyin, &GXDEPinyinEngine::preeditUpdate, &ui, &CandidateBar::updatePreedit);
    QObject::connect(&pinyin, &GXDEPinyinEngine::candidateUpdate, &ui, [&](const QStringList &cands, int selIdx){
        ui.updateCandidates(cands, selIdx);
        if(!cands.isEmpty()) ui.show();
        else ui.hide();
    });
    QObject::connect(&ui, &CandidateBar::candidateSelected, &pinyin, &GXDEPinyinEngine::selectCandidate);
    QObject::connect(&pinyin, &GXDEPinyinEngine::commitWord, &core, &GXDEInputCore::commitString);
    // 页码更新：候选栏显示 1/2 形式的页码
    QObject::connect(&pinyin, &GXDEPinyinEngine::pageInfoUpdate, &ui, &CandidateBar::updatePageInfo);

    // 信号绑定：方向键、翻页、数字选词
    QObject::connect(&core, &GXDEInputCore::keyUp, &pinyin, &GXDEPinyinEngine::selectPrev);
    QObject::connect(&core, &GXDEInputCore::keyDown, &pinyin, &GXDEPinyinEngine::selectNext);
    QObject::connect(&core, &GXDEInputCore::keyPageUp, &pinyin, &GXDEPinyinEngine::pageUp);
    QObject::connect(&core, &GXDEInputCore::keyPageDown, &pinyin, &GXDEPinyinEngine::pageDown);
    QObject::connect(&core, &GXDEInputCore::keyNumber, &pinyin, &GXDEPinyinEngine::selectCandidate);

    // xkbcommon 解析出的按键：字符进拼音预编辑，退格删除末位字符
    QObject::connect(&core, &GXDEInputCore::keyCharacter, &pinyin, [&pinyin](const QString &text){
        for (const QChar &ch : text) {
            const QChar lower = ch.toLower();
            if (lower >= QLatin1Char('a') && lower <= QLatin1Char('z'))
                pinyin.appendChar(lower.toLatin1());
        }
    });
    QObject::connect(&core, &GXDEInputCore::keyBackspace, &pinyin, &GXDEPinyinEngine::backspace);

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

    // 输入上下文激活与光标坐标不再本地模拟，
    // 由 gxde-wlcom 通过 DBus 调用 Activate / SetCursorPos 驱动

    qDebug() << "GXDE Input Engine with CandidateBar UI running...";
    return app.exec();
}
