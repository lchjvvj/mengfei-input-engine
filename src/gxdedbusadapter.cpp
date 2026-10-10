#include "gxdedbusadapter.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

#include "ui/SettingsPanel.hpp"

GXDEDBusAdapter::GXDEDBusAdapter(SettingsPanel *settingsPanel, QObject *parent)
    : QObject(parent), m_settingsPanel(settingsPanel)
{
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

void GXDEDBusAdapter::ShowSettings()
{
    m_settingsPanel->show();
}
