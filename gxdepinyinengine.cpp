#include "gxdepinyinengine.h"

// 极简内置词库，仅演示，正式需替换成大型词库
QMap<QString, QStringList> dict = {
    {"ni", {"你", "尼", "拟"}},
    {"hao", {"好", "浩", "耗"}},
    {"wo", {"我", "窝", "握"}},
    {"shi", {"是", "时", "事"}},
    {"gxde", {"GXDE"}},
};

GXDEPinyinEngine::GXDEPinyinEngine(QObject *parent)
    : QObject(parent)
{
}

void GXDEPinyinEngine::appendChar(char ch)
{
    m_pinyinBuf += ch;
    QStringList res = lookup(m_pinyinBuf);
    emit preeditUpdate(m_pinyinBuf);
    emit candidateUpdate(res);
}

void GXDEPinyinEngine::backspace()
{
    if (!m_pinyinBuf.isEmpty()) {
        m_pinyinBuf.chop(1);
    }
    QStringList res = lookup(m_pinyinBuf);
    emit preeditUpdate(m_pinyinBuf);
    emit candidateUpdate(res);
}

void GXDEPinyinEngine::clear()
{
    m_pinyinBuf.clear();
    emit preeditUpdate("");
    emit candidateUpdate({});
}

bool GXDEPinyinEngine::selectCandidate(int idx)
{
    QStringList cand = lookup(m_pinyinBuf);
    if (idx < 0 || idx >= cand.size())
        return false;
    QString word = cand[idx];
    emit commitWord(word);
    clear();
    return true;
}

QString GXDEPinyinEngine::getRawPinyin() const
{
    return m_pinyinBuf;
}

QStringList GXDEPinyinEngine::getCandidates() const
{
    return lookup(m_pinyinBuf);
}

QStringList GXDEPinyinEngine::lookup(const QString &py)
{
    if (py.isEmpty()) return {};
    if (dict.contains(py))
        return dict[py];
    return {};
}
