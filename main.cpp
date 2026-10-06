#include <QCoreApplication>
#include <QGuiApplication>
#include "gxdeinputcore.h"
#include "gxdepinyinengine.h"
#include "CandidateBar.hpp"
#include <QDebug>

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
    QObject::connect(&pinyin, &GXDEPinyinEngine::candidateUpdate, &ui, [&](const QStringList &cands){
        ui.updateCandidates(cands,0);
        if(!cands.isEmpty()) ui.show();
        else ui.hide();
    });
    QObject::connect(&ui, &CandidateBar::candidateSelected, &pinyin, &GXDEPinyinEngine::selectCandidate);
    QObject::connect(&pinyin, &GXDEPinyinEngine::commitWord, &core, &GXDEInputCore::commitString);

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
