#include "CandidateBar.hpp"
#include <QUrl>
#include <QQuickItem>

CandidateBar::CandidateBar(QObject *parent)
    : QObject(parent), m_view(new QQuickView())
{
    m_view->setSource(QUrl("qrc:/ui/CandidateBar.qml"));
    m_view->setResizeMode(QQuickView::SizeViewToRootObject);

    // 将当前C++对象暴露给QML，名称 candidateBarCpp
    QObject *root = m_view->rootObject();
    root->setProperty("candidateBarCpp", QVariant::fromValue(this));
}

CandidateBar::~CandidateBar()
{
    delete m_view;
}

void CandidateBar::updatePreedit(const QString &text)
{
    QObject *root = m_view->rootObject();
    root->setProperty("preeditText", text);
}

void CandidateBar::updateCandidates(const QStringList &list, int selIdx)
{
    QObject *root = m_view->rootObject();
    root->setProperty("candidates", list);
    root->setProperty("selectedIndex", selIdx);
}

void CandidateBar::setCursorPos(qreal x, qreal y)
{
    QObject *root = m_view->rootObject();
    root->setProperty("cursorX", x);
    root->setProperty("cursorY", y);
}

void CandidateBar::setDarkMode(bool dark)
{
    QObject *root = m_view->rootObject();
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
