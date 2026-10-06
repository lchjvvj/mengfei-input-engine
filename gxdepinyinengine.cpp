#include "gxdepinyinengine.h"

QMap<QString, QStringList> dict = {
    {"ni", {"你", "尼", "拟", "泥", "倪", "妮", "霓", "腻", "涅", "尼米兹"}},
    {"hao", {"好", "浩", "耗", "昊", "灏", "蒿", "嚯", "薅", "颢", "蚝"}},
    {"wo", {"我", "窝", "握", "沃", "蜗", "斡", "倭", "莴", "肟", "喔"}},
    {"shi", {"是", "时", "事", "实", "市", "史", "使", "始", "施", "师"}},
    {"gxde", {"GXDE", "GXDE-WLCOM", "FlakeWM", "Treeland"}},
};

GXDEPinyinEngine::GXDEPinyinEngine(QObject *parent)
    : QObject(parent)
{
}

void GXDEPinyinEngine::appendChar(char ch)
{
    m_pinyinBuf += ch;
    m_allCandidates = lookup(m_pinyinBuf);
    m_pageOffset = 0;
    m_selectedIndex = 0;
    refreshPage();
}

void GXDEPinyinEngine::backspace()
{
    if (!m_pinyinBuf.isEmpty()) {
        m_pinyinBuf.chop(1);
    }
    m_allCandidates = lookup(m_pinyinBuf);
    m_pageOffset = 0;
    m_selectedIndex = 0;
    refreshPage();
}

void GXDEPinyinEngine::clear()
{
    m_pinyinBuf.clear();
    m_allCandidates.clear();
    m_pageOffset = 0;
    m_selectedIndex = 0;
    emit preeditUpdate("");
    emit candidateUpdate({}, 0);
}

bool GXDEPinyinEngine::selectCandidate(int idx)
{
    int realIdx = m_pageOffset + idx;
    if (realIdx < 0 || realIdx >= m_allCandidates.size())
        return false;
    QString word = m_allCandidates[realIdx];
    emit commitWord(word);
    clear();
    return true;
}

// 上一个候选
void GXDEPinyinEngine::selectPrev()
{
    if (m_allCandidates.isEmpty()) return;
    if (m_selectedIndex <= 0) {
        // 到页首，尝试上一页
        if(m_pageOffset > 0) {
            m_pageOffset -= getPageSize();
            m_selectedIndex = getPageSize() - 1;
        }
    } else {
        m_selectedIndex--;
    }
    refreshPage();
}

// 下一个候选
void GXDEPinyinEngine::selectNext()
{
    if (m_allCandidates.isEmpty()) return;
    int pageEnd = qMin(m_pageOffset + getPageSize(), m_allCandidates.size());
    int curReal = m_pageOffset + m_selectedIndex;
    if(curReal >= pageEnd - 1) {
        // 当前页末尾，翻下一页
        if(m_pageOffset + getPageSize() < m_allCandidates.size()) {
            m_pageOffset += getPageSize();
            m_selectedIndex = 0;
        }
    } else {
        m_selectedIndex++;
    }
    refreshPage();
}

void GXDEPinyinEngine::pageUp()
{
    if(m_pageOffset <= 0) return;
    m_pageOffset -= getPageSize();
    m_selectedIndex = 0;
    refreshPage();
}

void GXDEPinyinEngine::pageDown()
{
    if(m_pageOffset + getPageSize() >= m_allCandidates.size()) return;
    m_pageOffset += getPageSize();
    m_selectedIndex = 0;
    refreshPage();
}

QString GXDEPinyinEngine::getRawPinyin() const
{
    return m_pinyinBuf;
}

QStringList GXDEPinyinEngine::getCandidates() const
{
    QStringList pageList;
    int end = qMin(m_pageOffset + getPageSize(), m_allCandidates.size());
    for(int i = m_pageOffset; i < end; i++){
        pageList << m_allCandidates[i];
    }
    return pageList;
}

int GXDEPinyinEngine::getSelectedIndex() const
{
    return m_selectedIndex;
}

QStringList GXDEPinyinEngine::lookup(const QString &py)
{
    if (py.isEmpty()) return {};
    if (dict.contains(py))
        return dict[py];
    return {};
}

// 刷新当前页候选列表，通知UI更新
void GXDEPinyinEngine::refreshPage()
{
    QStringList pageCand = getCandidates();
    emit preeditUpdate(m_pinyinBuf);
    emit candidateUpdate(pageCand, m_selectedIndex);
}
