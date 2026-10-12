#include "SettingsPanel.hpp"

#include <QDebug>
#include <QQmlContext>
#include <QUrl>

#include "../gxdesettings.h"

SettingsPanel::SettingsPanel(GXDESettings *settings, QObject *parent)
    : QObject(parent), m_view(new QQuickView())
{
    // settingsCpp：设置数据读写；settingsPanelCpp：QML 关闭面板回调。
    // 上下文属性必须在 setSource 之前注入，否则 QML 绑定解析失败
    m_view->rootContext()->setContextProperty("settingsCpp", settings);
    m_view->rootContext()->setContextProperty("settingsPanelCpp", this);
    m_view->setSource(QUrl("qrc:/ui/SettingsPanel.qml"));
    m_view->setResizeMode(QQuickView::SizeViewToRootObject);
    m_view->setTitle(QStringLiteral("梦飞输入法设置"));

    // 降级策略：QML 加载失败只告警，
    // 不影响引擎主体与 DBus 服务注册
    if (!m_view->rootObject())
        qWarning() << "[SettingsPanel] QML 根对象加载失败，设置面板不可用:"
                   << m_view->errors();
}

SettingsPanel::~SettingsPanel()
{
    delete m_view;
}

void SettingsPanel::show()
{
    m_view->show();
    m_view->raise();
    m_view->requestActivate();
}

void SettingsPanel::close()
{
    m_view->hide();
}
