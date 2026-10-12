#include "gxdesettings.h"

#include <QColor>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

// 设置文件路径：与 GXDEPinyinEngine 的 rime 用户目录同级
//（~/.config/mengfei-input-engine/），目录不存在时自动创建
QString settingsFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                        + QLatin1String("/mengfei-input-engine");
    QDir().mkpath(dir);
    return dir + QLatin1String("/settings.ini");
}

// ini 键名：按 设置面板分区/条目 组织
const char *const kKeyChineseEnabled = "input/chineseEnabled";
const char *const kKeyDarkMode       = "theme/darkMode";
const char *const kKeyAccentColor    = "theme/accentColor";
const char *const kKeyBarFontSize    = "candidateBar/fontSize";
const char *const kKeyBarSpacing     = "candidateBar/spacing";
const char *const kKeyShowPageInfo   = "candidateBar/showPageInfo";

// 默认值：与接入设置前的候选栏外观保持一致
constexpr bool kDefaultChineseEnabled = true;
constexpr bool kDefaultDarkMode = false;
constexpr int kDefaultBarFontSize = 16;
constexpr int kDefaultBarSpacing = 8;
constexpr bool kDefaultShowPageInfo = true;
const char *const kDefaultAccentColor = "#0078D4";

} // namespace

GXDESettings::GXDESettings(QObject *parent)
    : QObject(parent)
{
    load();
}

void GXDESettings::load()
{
    QSettings s(settingsFilePath(), QSettings::IniFormat);

    m_chineseEnabled = s.value(QLatin1String(kKeyChineseEnabled),
                               kDefaultChineseEnabled).toBool();
    m_darkMode = s.value(QLatin1String(kKeyDarkMode), kDefaultDarkMode).toBool();
    m_barFontSize = qBound(kMinFontSize,
                           s.value(QLatin1String(kKeyBarFontSize),
                                   kDefaultBarFontSize).toInt(),
                           kMaxFontSize);
    m_barSpacing = qBound(kMinSpacing,
                          s.value(QLatin1String(kKeyBarSpacing),
                                  kDefaultBarSpacing).toInt(),
                          kMaxSpacing);
    m_showPageInfo = s.value(QLatin1String(kKeyShowPageInfo),
                             kDefaultShowPageInfo).toBool();

    // 用户手改 ini 可能写入非法颜色串，校验失败回退默认高亮色
    //（QColor 构造解析失败时为无效颜色，等价于旧版 isValidColor 校验）
    const QString accent = s.value(QLatin1String(kKeyAccentColor),
                                   QLatin1String(kDefaultAccentColor)).toString();
    m_accentColor = QColor(accent).isValid() ? accent
                                             : QLatin1String(kDefaultAccentColor);
}

void GXDESettings::save(const char *key, const QVariant &value)
{
    QSettings s(settingsFilePath(), QSettings::IniFormat);
    s.setValue(QLatin1String(key), value);
}

bool GXDESettings::chineseEnabled() const
{
    return m_chineseEnabled;
}

void GXDESettings::setChineseEnabled(bool enabled)
{
    if (m_chineseEnabled == enabled)
        return;
    m_chineseEnabled = enabled;
    save(kKeyChineseEnabled, enabled);
    emit chineseEnabledChanged(enabled);
}

bool GXDESettings::darkMode() const
{
    return m_darkMode;
}

void GXDESettings::setDarkMode(bool dark)
{
    if (m_darkMode == dark)
        return;
    m_darkMode = dark;
    save(kKeyDarkMode, dark);
    emit darkModeChanged(dark);
}

int GXDESettings::barFontSize() const
{
    return m_barFontSize;
}

void GXDESettings::setBarFontSize(int px)
{
    px = qBound(kMinFontSize, px, kMaxFontSize);
    if (m_barFontSize == px)
        return;
    m_barFontSize = px;
    save(kKeyBarFontSize, px);
    emit barFontSizeChanged(px);
}

int GXDESettings::barSpacing() const
{
    return m_barSpacing;
}

void GXDESettings::setBarSpacing(int spacing)
{
    spacing = qBound(kMinSpacing, spacing, kMaxSpacing);
    if (m_barSpacing == spacing)
        return;
    m_barSpacing = spacing;
    save(kKeyBarSpacing, spacing);
    emit barSpacingChanged(spacing);
}

bool GXDESettings::showPageInfo() const
{
    return m_showPageInfo;
}

void GXDESettings::setShowPageInfo(bool show)
{
    if (m_showPageInfo == show)
        return;
    m_showPageInfo = show;
    save(kKeyShowPageInfo, show);
    emit showPageInfoChanged(show);
}

QString GXDESettings::accentColor() const
{
    return m_accentColor;
}

void GXDESettings::setAccentColor(const QString &color)
{
    if (!QColor(color).isValid())
        return;
    if (m_accentColor == color)
        return;
    m_accentColor = color;
    save(kKeyAccentColor, color);
    emit accentColorChanged(color);
}

void GXDESettings::resetDefaults()
{
    setChineseEnabled(kDefaultChineseEnabled);
    setDarkMode(kDefaultDarkMode);
    setBarFontSize(kDefaultBarFontSize);
    setBarSpacing(kDefaultBarSpacing);
    setShowPageInfo(kDefaultShowPageInfo);
    setAccentColor(QLatin1String(kDefaultAccentColor));
}
