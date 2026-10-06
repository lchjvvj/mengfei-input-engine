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

    // 信号绑定：方向键、翻页、数字选词
    QObject::connect(&core, &GXDEInputCore::keyUp, &pinyin, &GXDEPinyinEngine::selectPrev);
    QObject::connect(&core, &GXDEInputCore::keyDown, &pinyin, &GXDEPinyinEngine::selectNext);
    QObject::connect(&core, &GXDEInputCore::keyPageUp, &pinyin, &GXDEPinyinEngine::pageUp);
    QObject::connect(&core, &GXDEInputCore::keyPageDown, &pinyin, &GXDEPinyinEngine::pageDown);
    QObject::connect(&core, &GXDEInputCore::keyNumber, &pinyin, &GXDEPinyinEngine::selectCandidate);

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
