#ifndef GXDEPINYINENGINE_H
#define GXDEPINYINENGINE_H

#include <QObject>
#include <QStringList>

class GXDEPinyinEngine : public QObject
{
    Q_OBJECT
public:
    explicit GXDEPinyinEngine(QObject *parent = nullptr);

    void appendChar(char ch);
    void backspace();
    void clear();

    bool selectCandidate(int idx);
    // 新增：方向键、翻页
    void selectPrev();
    void selectNext();
    void pageUp();
    void pageDown();

    QString getRawPinyin() const;
    QStringList getCandidates() const;
    int getSelectedIndex() const;
    int getPageSize() const { return 9; } // 每页最多9个候选，对应数字1-9

signals:
    void preeditUpdate(const QString &pinyin);
    void candidateUpdate(const QStringList &cands, int selIdx);
    void commitWord(const QString &hanzi);

private:
    QString m_pinyinBuf;
    QStringList m_allCandidates; // 全部候选（不分页）
    int m_selectedIndex = 0;
    int m_pageOffset = 0; // 当前页偏移
    QStringList lookup(const QString &py);
    void refreshPage();
};

#endif // GXDEPINYINENGINE_H
