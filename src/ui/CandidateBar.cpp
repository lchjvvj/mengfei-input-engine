#include "CandidateBar.hpp"

#include <utility>

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QtMath>

#include "../gxdesettings.h"

namespace {
// 布局常量，与原 CandidateBar.qml 保持一致
constexpr qreal kPadding = 8.0;        // 四周留白
constexpr qreal kNumberSpacing = 4.0;  // 候选项内序号与候选词的间距
}

CandidateBar::CandidateBar(GXDESettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
    // 设置面板修改深浅色/字号/间距/页码/高亮色后即时重绘生效
    connect(m_settings, &GXDESettings::darkModeChanged, this, [this] { render(); });
    connect(m_settings, &GXDESettings::barFontSizeChanged, this, [this] { render(); });
    connect(m_settings, &GXDESettings::barSpacingChanged, this, [this] { render(); });
    connect(m_settings, &GXDESettings::showPageInfoChanged, this, [this] { render(); });
    connect(m_settings, &GXDESettings::accentColorChanged, this, [this] { render(); });
}

void CandidateBar::updatePreedit(const QString &text)
{
    m_preedit = text;
    if (m_visible)
        render();
}

void CandidateBar::updateCandidates(const QStringList &list, int selIdx)
{
    m_candidates = list;
    m_selectedIndex = selIdx;
    if (m_visible)
        render();
}

void CandidateBar::updatePageInfo(int currentPage, int totalPages)
{
    m_currentPage = currentPage;
    m_totalPages = totalPages;
    if (m_visible)
        render();
}

void CandidateBar::setDarkMode(bool dark)
{
    // 设置为单一数据源：写入 GXDESettings 后由其 notify 信号驱动重绘
    if (m_settings)
        m_settings->setDarkMode(dark);
}

void CandidateBar::setDevicePixelRatio(int ratio)
{
    if (ratio < 1)
        ratio = 1;
    if (m_devicePixelRatio == ratio)
        return;
    m_devicePixelRatio = ratio;
    if (m_visible)
        render();
}

void CandidateBar::hide()
{
    // 已隐藏时不重复发空帧，避免 popup surface 反复提交空缓冲
    if (!m_visible)
        return;
    m_visible = false;
    render();
}

void CandidateBar::show()
{
    m_visible = true;
    render();
}

void CandidateBar::render()
{
    if (!m_visible) {
        emit frameChanged(QImage());
        return;
    }

    const bool dark = m_settings->darkMode();
    const bool showPage = m_settings->showPageInfo() && m_totalPages > 1;
    const qreal spacing = m_settings->barSpacing();

    QFont preeditFont;
    preeditFont.setPixelSize(m_settings->barFontSize() + 2);
    QFont candidateFont;
    candidateFont.setPixelSize(m_settings->barFontSize());
    QFont numberFont = candidateFont;
    numberFont.setBold(true);
    QFont pageFont;
    pageFont.setPixelSize(m_settings->barFontSize() - 2);

    const QFontMetricsF preeditMetrics(preeditFont);
    const QFontMetricsF candidateMetrics(candidateFont);
    const QFontMetricsF numberMetrics(numberFont);
    const QFontMetricsF pageMetrics(pageFont);

    // 候选项：序号（"N."）与词的宽度先量好，绘制时直接用
    struct CandidateItem
    {
        QString number;
        QString text;
        qreal numberWidth;
        qreal textWidth;
        bool selected;
    };
    QList<CandidateItem> items;
    items.reserve(m_candidates.size());
    for (int i = 0; i < m_candidates.size(); ++i) {
        CandidateItem item;
        item.number = QString::number(i + 1) + QLatin1Char('.');
        item.text = m_candidates.at(i);
        item.numberWidth = numberMetrics.horizontalAdvance(item.number);
        item.textWidth = candidateMetrics.horizontalAdvance(item.text);
        item.selected = (i == m_selectedIndex);
        items.append(item);
    }
    const QString pageText = showPage
        ? QStringLiteral("%1/%2").arg(m_currentPage).arg(m_totalPages)
        : QString();

    // 无任何可绘制内容（preedit/候选/页码均空）时等同隐藏，
    // 避免提交只有一块底色的空候选窗
    if (m_preedit.isEmpty() && items.isEmpty()) {
        emit frameChanged(QImage());
        return;
    }

    // 画布尺寸：宽 = 留白*2 + Σ项目宽 + 项目间距*(项目数-1)，
    // 项目宽对候选项为 序号宽+4+词宽；高 = 留白*2 + 参与绘制字体的最大行高
    qreal contentWidth = 0;
    int itemCount = 0;
    qreal ascent = 0;
    qreal contentHeight = 0;
    auto accumulateFont = [&](const QFontMetricsF &metrics) {
        ascent = qMax(ascent, metrics.ascent());
        contentHeight = qMax(contentHeight, metrics.ascent() + metrics.descent());
    };
    if (!m_preedit.isEmpty()) {
        contentWidth += preeditMetrics.horizontalAdvance(m_preedit);
        ++itemCount;
        accumulateFont(preeditMetrics);
    }
    for (const CandidateItem &item : std::as_const(items)) {
        contentWidth += item.numberWidth + kNumberSpacing + item.textWidth;
        ++itemCount;
        accumulateFont(numberMetrics);
        accumulateFont(candidateMetrics);
    }
    if (!pageText.isEmpty()) {
        contentWidth += pageMetrics.horizontalAdvance(pageText);
        ++itemCount;
        accumulateFont(pageMetrics);
    }
    contentWidth += spacing * (itemCount - 1);

    const qreal logicalWidth = contentWidth + kPadding * 2;
    const qreal logicalHeight = contentHeight + kPadding * 2;
    const int width = qMax(1, qCeil(logicalWidth * m_devicePixelRatio));
    const int height = qMax(1, qCeil(logicalHeight * m_devicePixelRatio));

    QImage frame(width, height, QImage::Format_ARGB32_Premultiplied);
    frame.setDevicePixelRatio(m_devicePixelRatio);
    frame.fill(dark ? QColor(QStringLiteral("#2B2B2B"))
                    : QColor(QStringLiteral("#FFFFFF")));

    // 所有文字按基线对齐：基线 = 上留白 + 参与绘制字体的最大 ascent，
    // 绘制坐标使用逻辑像素，QPainter 按 devicePixelRatio 缩放
    QPainter painter(&frame);
    painter.setRenderHint(QPainter::TextAntialiasing);
    const qreal baseline = kPadding + ascent;
    const QColor accent(m_settings->accentColor());
    qreal x = kPadding;
    if (!m_preedit.isEmpty()) {
        painter.setFont(preeditFont);
        painter.setPen(dark ? QColor(QStringLiteral("#EEEEEE"))
                            : QColor(QStringLiteral("#111111")));
        painter.drawText(QPointF(x, baseline), m_preedit);
        x += preeditMetrics.horizontalAdvance(m_preedit) + spacing;
    }
    for (const CandidateItem &item : std::as_const(items)) {
        painter.setFont(numberFont);
        painter.setPen(item.selected ? accent : QColor(QStringLiteral("#666666")));
        painter.drawText(QPointF(x, baseline), item.number);
        x += item.numberWidth + kNumberSpacing;
        painter.setFont(candidateFont);
        painter.setPen(item.selected ? accent
                                     : dark ? QColor(QStringLiteral("#DDDDDD"))
                                            : QColor(QStringLiteral("#222222")));
        painter.drawText(QPointF(x, baseline), item.text);
        x += item.textWidth + spacing;
    }
    if (!pageText.isEmpty()) {
        painter.setFont(pageFont);
        painter.setPen(QColor(QStringLiteral("#888888")));
        painter.drawText(QPointF(x, baseline), pageText);
    }
    painter.end();

    emit frameChanged(frame);
}
