#include <QGuiApplication>
#include <QDebug>

#include "gxdeinputcore.h"
#include "gxdepinyinengine.h"
#include "ui/CandidateBar.hpp"

int main(int argc, char *argv[])
{
    // QML窗口必须使用QGuiApplication，不能QCoreApplication！
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("gxde-input-engine");

    GXDEInputCore core;
    GXDEPinyinEngine pinyin;
    CandidateBar ui;

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

    // 输入上下文激活时显示UI，失活隐藏
    QObject::connect(&core, &GXDEInputCore::preeditChanged, &ui, &CandidateBar::updatePreedit);
    QObject::connect(&core, &GXDEInputCore::candidateListChanged, &ui, &CandidateBar::updateCandidates);
    QObject::connect(&core, &GXDEInputCore::textCommitted, &ui, &CandidateBar::hide);

    // 模拟激活输入框
    core.activateContext();
    // 模拟光标位置（真实场景由gxde-wlcom通过DBus传入）
    ui.setCursorPos(300,200);

    qDebug() << "GXDE Input Engine with CandidateBar UI running...";
    return app.exec();
}
