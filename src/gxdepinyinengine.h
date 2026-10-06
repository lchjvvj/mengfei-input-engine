#ifndef GXDEPINYINENGINE_H
#define GXDEPINYINENGINE_H

#include <QObject>
#include <QStringList>

// 拼音引擎：基于 librime（Rime 中州韵）的方案后端，替代旧版写死的 QMap 简易词库。
// 词库、候选排序、分页与选词行为由 Rime 方案决定（默认 luna_pinyin，
// 由 librime-data 提供）；本类只负责把按键转发给 Rime，并把 Rime 的
// 上下文（preedit/候选/分页/上屏文本）翻译成候选栏信号。
// 对外接口与信号和旧版保持一致，main.cpp / DBus 适配层无需改动。
class GXDEPinyinEngine : public QObject
{
    Q_OBJECT
public:
    explicit GXDEPinyinEngine(QObject *parent = nullptr);
    ~GXDEPinyinEngine() override;

    void appendChar(char ch);
    void backspace();
    void clear();

    bool selectCandidate(int idx);
    // 方向键、翻页：转发给 Rime 按键处理，行为跟随方案 key_binder 配置
    void selectPrev();
    void selectNext();
    void pageUp();
    void pageDown();

    QString getRawPinyin() const;      // Rime 预编辑串（已分音节，如 "ni hao"）
    QStringList getCandidates() const; // 当前页候选
    int getSelectedIndex() const;      // 页内高亮候选索引
    int getPageSize() const;           // 由方案 menu/page_size 决定（Rime 默认 5）
    // 页码信息（1起始）；无候选时均为0，供UI显示 "1/2" 页码
    int getCurrentPage() const;
    int getTotalPages() const;         // librime 不提供总页数，按"是否最后一页"推算

signals:
    void preeditUpdate(const QString &pinyin);
    void candidateUpdate(const QStringList &cands, int selIdx);
    void pageInfoUpdate(int currentPage, int totalPages);
    void commitWord(const QString &hanzi);

private:
    void sendKey(int keysym); // 单个 keysym（XKB_KEY_*）转发给 Rime 并同步状态
    void syncState();         // 读取上屏文本与 RimeContext，更新缓存并发信号

    class Private;
    Private *d;
};

#endif // GXDEPINYINENGINE_H
