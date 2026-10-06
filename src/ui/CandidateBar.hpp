#ifndef CANDIDATEBAR_HPP
#define CANDIDATEBAR_HPP

#include <QObject>
#include <QQuickView>
#include <QStringList>

class CandidateBar : public QObject
{
    Q_OBJECT
public:
    explicit CandidateBar(QObject *parent = nullptr);
    ~CandidateBar() override;

    // 更新候选栏数据
    void updatePreedit(const QString &text);
    void updateCandidates(const QStringList &list, int selIdx);
    void updatePageInfo(int currentPage, int totalPages);
    void setCursorPos(qreal x, qreal y);
    void setDarkMode(bool dark);
    void hide();
    void show();

signals:
    // QML点击候选词后，触发信号交给拼音引擎处理
    void candidateSelected(int index);

public slots:
    void selectCandidate(int idx);

private:
    QQuickView *m_view;
};

#endif // CANDIDATEBAR_HPP
