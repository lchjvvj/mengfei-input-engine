#ifndef CANDIDATEBAR_HPP
#define CANDIDATEBAR_HPP

#include <QImage>
#include <QObject>
#include <QStringList>

class GXDESettings;

// 候选栏渲染器：把 preedit/候选词/页码按 GXDESettings 样式绘制成一帧
// QImage（物理像素，Format_ARGB32_Premultiplied），自身不创建窗口。
// GXDEWaylandIM 通过 frameChanged 信号取帧，拷贝进 wl_shm 缓冲后经
// zwp_input_popup_surface_v2 提交显示；候选窗位置由合成器负责——
// 贴着 text-input-v3 set_cursor_rectangle 上报的文本输入矩形放置并做
// 屏幕边界避让，引擎不再接收光标坐标、不再自行定位窗口。
// 渲染规则与原 CandidateBar.qml 一致：横向单行、四周留白 8、项目间距
// barSpacing、候选项内序号与词间距 4，高亮项用 accentColor。
class CandidateBar : public QObject
{
    Q_OBJECT
public:
    // settings：全局设置（主题/候选栏样式），样式变更经 notify 信号驱动重绘
    explicit CandidateBar(GXDESettings *settings, QObject *parent = nullptr);

    // 更新候选栏数据（可见时立即重绘并发出新帧）
    void updatePreedit(const QString &text);
    void updateCandidates(const QStringList &list, int selIdx);
    void updatePageInfo(int currentPage, int totalPages);
    // 深色主题切换入口：统一写入 GXDESettings，notify 信号驱动重绘
    //（后续接入 gxde-theme-daemon 主题联动时调用此接口即可）
    void setDarkMode(bool dark);
    // 显示/隐藏：隐藏发出空帧，popup surface 提交空缓冲后候选窗不可见
    void hide();
    void show();

    // 输出缩放系数（wl_output scale，由 GXDEWaylandIM 依据 popup surface
    // 进入的输出通知）：按物理像素渲染，帧的 devicePixelRatio 与之相同，
    // 配合 wl_surface_set_buffer_scale 保证 HiDPI 下清晰
    void setDevicePixelRatio(int ratio);

signals:
    // 新一帧候选窗内容；空（isNull）图像表示候选栏隐藏
    void frameChanged(const QImage &frame);

private:
    // 依据当前数据与设置重新布局绘制，发出 frameChanged
    void render();

    GXDESettings *m_settings;
    QString m_preedit;
    QStringList m_candidates;
    int m_selectedIndex = 0;
    int m_currentPage = 0;
    int m_totalPages = 0;
    bool m_visible = false;
    int m_devicePixelRatio = 1;
};

#endif // CANDIDATEBAR_HPP
