#include "CandidateBar.hpp"
#include <QDebug>
#include <QUrl>
#include <QQuickItem>

CandidateBar::CandidateBar(QObject *parent)
    : QObject(parent), m_view(new QQuickView())
{
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

void CandidateBar::setCursorPos(qreal x, qreal y)
{
    QObject *root = m_view->rootObject();
    if (!root) return;
    root->setProperty("cursorX", x);
    root->setProperty("cursorY", y);
}

void CandidateBar::setDarkMode(bool dark)
{
    QObject *root = m_view->rootObject();
    if (!root) return;
    root->setProperty("darkMode", dark);
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
