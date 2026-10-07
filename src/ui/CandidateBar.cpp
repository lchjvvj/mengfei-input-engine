#include "CandidateBar.hpp"
#include <QDebug>
#include <QQmlContext>
#include <QUrl>
#include <QQuickItem>

#include "../gxdesettings.h"

CandidateBar::CandidateBar(GXDESettings *settings, QObject *parent)
    : QObject(parent), m_view(new QQuickView()), m_settings(settings)
{
    // 全局设置以 settingsCpp 注入 QML：主题与候选栏样式
    //（字号/间距/页码/高亮色）在 QML 内声明式绑定，改设置即生效。
    // 上下文属性必须在 setSource 之前设置，否则绑定解析失败
    m_view->rootContext()->setContextProperty("settingsCpp", settings);
    m_view->setSource(QUrl("qrc:/ui/CandidateBar.qml"));
    m_view->setResizeMode(QQuickView::SizeViewToRootObject);

    // 将当前C++对象暴露给QML，名称 candidateBarCpp
    // 注意：QML 加载失败时（如以 Window 作根项，QQuickView 不支持）
    // rootObject() 为 nullptr，判空降级，避免引擎启动即崩溃导致 DBus 服务无法注册
    QObject *root = m_view->rootObject();
    if (!root) {
        qWarning() << "[CandidateBar] QML 根对象加载失败，候选栏UI不可用:" << m_view->errors();
        return;
    }
    root->setProperty("candidateBarCpp", QVariant::fromValue(this));
}

CandidateBar::~CandidateBar()
{
    delete m_view;
}

void CandidateBar::updatePreedit(const QString &text)
{
    QObject *root = m_view->rootObject();
    if (!root) return;
    root->setProperty("preeditText", text);
}

void CandidateBar::updateCandidates(const QStringList &list, int selIdx)
{
    QObject *root = m_view->rootObject();
    if (!root) return;
    root->setProperty("candidates", list);
    root->setProperty("selectedIndex", selIdx);
}

void CandidateBar::updatePageInfo(int currentPage, int totalPages)
{
    QObject *root = m_view->rootObject();
    if (!root) return;
    root->setProperty("currentPage", currentPage);
    root->setProperty("totalPages", totalPages);
}

void CandidateBar::setCursorRect(qreal x, qreal y, qreal width, qreal height)
{
    QObject *root = m_view->rootObject();
    if (!root) return;
    // 候选栏左对齐矩形左缘、宽度随内容自适应，width 不参与定位；
    // height 决定纵向偏移（紧贴矩形下缘，空矩形时 QML 回退默认值）
    Q_UNUSED(width)
    root->setProperty("cursorX", x);
    root->setProperty("cursorY", y);
    root->setProperty("cursorHeight", height);
}

void CandidateBar::setDarkMode(bool dark)
{
    // 设置为单一数据源：写入 GXDESettings 后由其 notify 信号驱动 QML 绑定更新
    if (m_settings)
        m_settings->setDarkMode(dark);
}

void CandidateBar::hide()
{
    m_view->hide();
}

void CandidateBar::show()
{
    m_view->show();
}

void CandidateBar::selectCandidate(int idx)
{
    emit candidateSelected(idx);
}
