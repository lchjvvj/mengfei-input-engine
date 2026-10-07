#ifndef CANDIDATEBAR_HPP
#define CANDIDATEBAR_HPP

#include <QObject>
#include <QQuickView>
#include <QStringList>

class GXDESettings;

class CandidateBar : public QObject
{
    Q_OBJECT
public:
    // settings：全局设置（主题/候选栏样式），以 settingsCpp 上下文属性
    // 注入 QML，样式绑定即时生效并随设置面板修改自动更新
    explicit CandidateBar(GXDESettings *settings, QObject *parent = nullptr);
    ~CandidateBar() override;

    // 更新候选栏数据
    void updatePreedit(const QString &text);
    void updateCandidates(const QStringList &list, int selIdx);
    void updatePageInfo(int currentPage, int totalPages);
    // text-input-v3 光标矩形（全局坐标）：候选栏紧贴矩形左下角定位，
    // width/height 为 0 表示空矩形，QML 回退默认行高偏移
    void setCursorRect(qreal x, qreal y, qreal width, qreal height);
    // 深色主题切换入口：统一写入 GXDESettings，QML 绑定自动更新配色
    //（后续接入 gxde-theme-daemon 主题联动时调用此接口即可）
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
    GXDESettings *m_settings;
};

#endif // CANDIDATEBAR_HPP
