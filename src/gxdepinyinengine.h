#ifndef GXDEPINYINENGINE_H
#define GXDEPINYINENGINE_H

#include <QObject>
#include <QStringList>

// 拼音引擎：基于 librime（Rime 中州韵）的方案后端，替代旧版写死的 QMap 简易词库。
// 词库、候选排序、分页与选词行为由 Rime 方案决定（默认 luna_pinyin，
// 由 librime-data 提供）；本类只负责把按键转发给 Rime，并把 Rime 的
// 上下文（preedit/候选/分页/上屏文本）翻译成候选栏信号。
// 按键是否被输入法消费以 Rime 的处理结果为准（processKey 的返回值），
// GXDEInputCore 据此决定消费还是把按键透传给焦点应用。
class GXDEPinyinEngine : public QObject
{
    Q_OBJECT
public:
    explicit GXDEPinyinEngine(QObject *parent = nullptr);
    ~GXDEPinyinEngine() override;

    void backspace();
    void clear();

    // 是否存在未完成的组字（preedit 或候选非空）：GXDEInputCore 据此判定
    // 方向键/翻页/退格/数字是组字操作（消费）还是应透传给应用
    bool isComposing() const;
    // 把 keysym 交给 librime 处理，返回 Rime 是否消费该按键：字母组字、
    // 标点转全角上屏、组字中空格选词、Enter 上屏原始输入等均被消费；
    // Rime 不关心的按键（无组字时的 Enter/Tab/Esc/空格、功能键等）返回
    // false，调用方应把按键透传给焦点应用。引擎未就绪时一律返回 false
    bool processKey(int keysym);

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
