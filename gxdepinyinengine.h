#ifndef GXDEPINYINENGINE_H
#define GXDEPINYINENGINE_H

#include <QObject>
#include <QStringList>

class GXDEPinyinEngine : public QObject
{
    Q_OBJECT
public:
    explicit GXDEPinyinEngine(QObject *parent = nullptr);

    // 输入拼音字符
    void appendChar(char ch);
    void backspace();
    void clear();
    // 选候选词
    bool selectCandidate(int idx);

    QString getRawPinyin() const;
    QStringList getCandidates() const;

signals:
    void preeditUpdate(const QString &pinyin);
    void candidateUpdate(const QStringList &cands);
    void commitWord(const QString &hanzi);

private:
    QString m_pinyinBuf;
    QStringList lookup(const QString &py);
};

#endif // GXDEPINYINENGINE_H
