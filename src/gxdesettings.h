#ifndef GXDESETTINGS_H
#define GXDESETTINGS_H

#include <QObject>
#include <QString>
#include <QVariant>

// 输入法全局设置：设置面板（QML）、候选栏与输入核心共用的唯一数据源。
// 通过 QSettings(ini) 持久化，文件位于 ~/.config/gxde-input-engine/settings.ini
//（与 rime 用户数据目录同级）。包含三类配置，与设置面板一一对应：
//   - 中英文切换：chineseEnabled（关闭后引擎不消费按键，直接上屏英文）
//   - 候选栏样式：barFontSize / barSpacing / showPageInfo
//   - 主题配置：darkMode / accentColor（候选高亮色）
class GXDESettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool chineseEnabled READ chineseEnabled WRITE setChineseEnabled NOTIFY chineseEnabledChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(int barFontSize READ barFontSize WRITE setBarFontSize NOTIFY barFontSizeChanged)
    Q_PROPERTY(int barSpacing READ barSpacing WRITE setBarSpacing NOTIFY barSpacingChanged)
    Q_PROPERTY(bool showPageInfo READ showPageInfo WRITE setShowPageInfo NOTIFY showPageInfoChanged)
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
public:
    explicit GXDESettings(QObject *parent = nullptr);

    bool chineseEnabled() const;
    void setChineseEnabled(bool enabled);

    bool darkMode() const;
    void setDarkMode(bool dark);

    int barFontSize() const;
    void setBarFontSize(int px);

    int barSpacing() const;
    void setBarSpacing(int spacing);

    bool showPageInfo() const;
    void setShowPageInfo(bool show);

    QString accentColor() const;
    void setAccentColor(const QString &color);

    // 候选栏字号/间距允许范围，setter 内部钳制，设置面板滑块使用同一区间
    static constexpr int kMinFontSize = 12;
    static constexpr int kMaxFontSize = 28;
    static constexpr int kMinSpacing = 2;
    static constexpr int kMaxSpacing = 24;

public slots:
    // 全部设置恢复默认值（设置面板"恢复默认"按钮）
    void resetDefaults();

signals:
    void chineseEnabledChanged(bool enabled);
    void darkModeChanged(bool dark);
    void barFontSizeChanged(int px);
    void barSpacingChanged(int spacing);
    void showPageInfoChanged(bool show);
    void accentColorChanged(const QString &color);

private:
    void load();
    void save(const char *key, const QVariant &value);

    bool m_chineseEnabled;
    bool m_darkMode;
    int m_barFontSize;
    int m_barSpacing;
    bool m_showPageInfo;
    QString m_accentColor;
};

#endif // GXDESETTINGS_H
