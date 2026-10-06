#include <QCoreApplication>
#include "gxdeinputcore.h"
#include "gxdepinyinengine.h"
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("gxde-input-engine");

    GXDEInputCore core;
    GXDEPinyinEngine pinyin;

    // 连接信号槽：拼音引擎 <-> 输入核心
    QObject::connect(&pinyin, &GXDEPinyinEngine::preeditUpdate, &core, &GXDEInputCore::setPreedit);
    QObject::connect(&pinyin, &GXDEPinyinEngine::candidateUpdate, &core, &GXDEInputCore::setCandidates);
    QObject::connect(&pinyin, &GXDEPinyinEngine::commitWord, &core, &GXDEInputCore::commitString);

    // 模拟激活输入框
    core.activateContext();

    // 模拟按键输入测试
    pinyin.appendChar('n');
    pinyin.appendChar('i');
    qDebug() << "候选词列表:" << pinyin.getCandidates();
    pinyin.selectCandidate(0); // 选择"你"

    qDebug() << "GXDE Input Engine running...";
    return app.exec();
}
