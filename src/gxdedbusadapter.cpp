#include "gxdedbusadapter.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

#include "gxdeinputcore.h"
#include "gxdepinyinengine.h"
#include "ui/CandidateBar.hpp"

GXDEDBusAdapter::GXDEDBusAdapter(GXDEInputCore *core, GXDEPinyinEngine *pinyin,
                                 CandidateBar *ui, QObject *parent)
    : QObject(parent), m_core(core), m_pinyin(pinyin), m_ui(ui)
{
    // 出站通道：引擎状态变化 -> DBus 信号回传给 gxde-wlcom
    connect(m_pinyin, &GXDEPinyinEngine::preeditUpdate, this,
            [this](const QString &text) { emit PreeditChanged(text, text.length()); });
    connect(m_pinyin, &GXDEPinyinEngine::candidateUpdate,
            this, &GXDEDBusAdapter::CandidateListChanged);
    connect(m_pinyin, &GXDEPinyinEngine::pageInfoUpdate,
            this, &GXDEDBusAdapter::PageInfoChanged);
    // 提交文本走 GXDEInputCore（text-input-v3 状态封装），保证与
    // commitString -> textCommitted 的上屏链路一致
    connect(m_core, &GXDEInputCore::textCommitted,
            this, &GXDEDBusAdapter::CommitText);
}

bool GXDEDBusAdapter::registerService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "[GXDEDBus] 无法连接 session bus";
        return false;
    }

    // 先注册对象再抢服务名：客户端看到名字时对象一定已就绪
    if (!bus.registerObject(kObjectPath, this,
                            QDBusConnection::ExportAllSlots |
                            QDBusConnection::ExportAllSignals)) {
        qCritical() << "[GXDEDBus] 注册对象失败:" << kObjectPath
                    << bus.lastError().message();
        return false;
    }

    if (!bus.registerService(kServiceName)) {
        qCritical() << "[GXDEDBus] 注册服务失败:" << kServiceName
                    << bus.lastError().message();
        bus.unregisterObject(kObjectPath);
        return false;
    }

    qDebug() << "[GXDEDBus] 服务已注册:" << kServiceName << kObjectPath;
    return true;
}

void GXDEDBusAdapter::Activate()
{
    m_core->activateContext();
}

void GXDEDBusAdapter::Deactivate()
{
    // 先清拼音状态（触发空 preedit/候选回传，避免候选栏残留），
    // 再关闭上下文并隐藏候选栏
    m_pinyin->clear();
    m_core->deactivateContext();
    m_ui->hide();
}

void GXDEDBusAdapter::Reset()
{
    m_pinyin->clear();
}

bool GXDEDBusAdapter::ProcessKeyEvent(uint keycode, bool pressed)
{
    return m_core->handleKeyEvent(keycode, pressed);
}

void GXDEDBusAdapter::SetCursorPos(int x, int y)
{
    m_ui->setCursorPos(x, y);
}
