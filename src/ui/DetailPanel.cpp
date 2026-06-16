#include "DetailPanel.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFontMetrics>
#include <QImageReader>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QTextOption>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>
#include "ui/Codicon.h"
#include "ui/ColorConstants.h"
#include "ui/ToastNotification.h"
#include "ui/UIUtils.h"
#include "ui/VisualConstants.h"

namespace {
QString detailMetaLine(const Asset &asset)
{
    QStringList parts;
    if (asset.width > 0 && asset.height > 0)
        parts << QStringLiteral("%1 x %2").arg(asset.width).arg(asset.height);
    if (!asset.format.isEmpty())
        parts << asset.format.toUpper();
    if (asset.fileSize > 0)
        parts << QStringLiteral("%1 KB").arg(qMax<qint64>(1, asset.fileSize / 1024));
    return parts.join("  |  ");
}
}

DetailPanel::DetailPanel(IImageCache *cache, QWidget *parent)
    : QWidget(parent), m_cache(cache)
{
    setMouseTracking(true);
    setMinimumWidth(340);

    m_promptEditor = new QPlainTextEdit(this);
    m_promptEditor->setVisible(false);
    m_promptEditor->setFrameShape(QFrame::NoFrame);
    m_promptEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_promptEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_promptEditor->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #252525;"
        "  color: #e0e0e0;"
        "  border: 1px solid #4a6cf7;"
        "  border-radius: 4px;"
        "  padding: 8px;"
        "  font-size: 12px;"
        "  selection-background-color: #4a6cf7;"
        "  selection-color: #ffffff;"
        "}"
    );
    m_promptEditor->installEventFilter(this);

    QFont base = font();
    m_fontBody = base; m_fontBody.setPixelSize(Visual::FontBody);
    m_fontMeta = base; m_fontMeta.setPixelSize(Visual::FontMeta);
    m_fontTitle = base; m_fontTitle.setPixelSize(Visual::FontTitle); m_fontTitle.setBold(true);
    m_fontCaption = base; m_fontCaption.setPixelSize(Visual::FontCaption);
    m_fontControl = base; m_fontControl.setPixelSize(Visual::FontControl); m_fontControl.setBold(true);
}

void DetailPanel::showAsset(const Asset &asset, const Metadata &metadata, const QVector<Tag> &tags)
{
    if (m_isEditingPrompt)
        finishPromptEdit(true);
    m_asset = asset;
    m_metadata = metadata;
    m_tags = tags;
    m_zoom = 1.0;
    m_panOffset = {};
    m_scrollOffset = 0;

    QImageReader reader(asset.filePath);
    QSize imgSize = reader.size();
    if (imgSize.isValid()) {
        QSize maxSize(1920, 1080);
        if (imgSize.width() > maxSize.width() || imgSize.height() > maxSize.height())
            reader.setScaledSize(imgSize.scaled(maxSize, Qt::KeepAspectRatio));
    }
    QImage img = reader.read();
    m_fullImage = img.isNull() ? QPixmap() : QPixmap::fromImage(img);
    update();
}

void DetailPanel::clear()
{
    if (m_isEditingPrompt) {
        m_isEditingPrompt = false;
        m_promptEditor->setVisible(false);
    }
    m_asset = {};
    m_metadata = {};
    m_tags.clear();
    m_fullImage = QPixmap();
    m_closeBtnRect = QRect();
    m_copyPromptRect = QRect();
    m_promptContentRect = QRect();
    m_copyFileNameRect = QRect();
    m_openFolderRect = QRect();
    m_openPreviewRect = QRect();
    m_scrollOffset = 0;
    m_maxScrollOffset = 0;
    update();
}

QRect DetailPanel::imageArea() const
{
    return QRect(0, 0, width(), qBound(260, (height() * 2) / 5, 420));
}

void DetailPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), Color::BG_DARK);

    if (m_asset.id.isEmpty()) {
    p.setFont(m_fontBody);
    Codicon::draw(p, "info", QRect(width() / 2 - 18, height() / 2 - 48, 36, 36),
                      Color::TEXT_SECONDARY, 28);
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(rect().adjusted(24, 18, -24, 0), Qt::AlignCenter, tr("选择一张素材查看详情"));
        return;
    }

    m_closeBtnRect = QRect(width() - 34, 6, 26, 26);
    if (m_closeBtnHovered) {
        QColor closeBg = Color::CLOSE_HOVER;
        closeBg.setAlpha(30);
        p.fillRect(m_closeBtnRect, closeBg);
    }
    Codicon::draw(p, "close", m_closeBtnRect, m_closeBtnHovered ? Color::CLOSE_HOVER : Color::TEXT_SECONDARY, 15);

    int imageBottom = drawImage(p);
    int summaryBottom = drawAssetSummary(p, imageBottom + 1);
    drawSectionDivider(p, summaryBottom);

    int scrollTop = summaryBottom + 1;
    int scrollHeight = height() - scrollTop;

    p.save();
    p.setClipRect(0, scrollTop, width(), scrollHeight);

    int contentY = scrollTop + 14 - m_scrollOffset;
    int y = drawMetadataSection(p, contentY);
    drawSectionDivider(p, y);
    y = drawFileInfo(p, y + 18);
    drawSectionDivider(p, y);
    y = drawTagsSection(p, y + 18);

    p.restore();

    int totalContent = y - scrollTop + 16;
    m_maxScrollOffset = qMax(0, totalContent - scrollHeight);
    clampScrollOffset();
    drawScrollIndicator(p);
}

void DetailPanel::drawSectionDivider(QPainter &p, int y) const
{
    p.setPen(Color::BORDER_MUTED);
    p.drawLine(0, y, width(), y);
}

int DetailPanel::drawImage(QPainter &p)
{
    QRect imgRect = imageArea();
    p.fillRect(imgRect, Color::BG_PREVIEW);

    if (m_fullImage.isNull()) {
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(imgRect, Qt::AlignCenter, tr("无法加载图片"));
        return imgRect.bottom();
    }

    double scale = m_zoom * qMin(static_cast<double>(imgRect.width()) / m_fullImage.width(),
                                 static_cast<double>(imgRect.height()) / m_fullImage.height());
    int drawW = static_cast<int>(m_fullImage.width() * scale);
    int drawH = static_cast<int>(m_fullImage.height() * scale);
    int imgX = imgRect.center().x() - drawW / 2 + m_panOffset.x();
    int imgY = imgRect.center().y() - drawH / 2 + m_panOffset.y();
    p.drawPixmap(imgX, imgY, drawW, drawH, m_fullImage);

    QRect infoBg(imgRect.left(), imgRect.bottom() - 30, imgRect.width(), 30);
    QLinearGradient grad(infoBg.topLeft(), infoBg.topRight());
    grad.setColorAt(0.0, Color::OVERLAY_LIGHT);
    grad.setColorAt(1.0, Qt::transparent);
    p.fillRect(infoBg, grad);
    p.setFont(m_fontMeta);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(infoBg.adjusted(10, 0, -34, 0), Qt::AlignVCenter,
               QStringLiteral("%1 x %2  |  %3%").arg(m_asset.width).arg(m_asset.height).arg(static_cast<int>(m_zoom * 100)));

    m_favStarRect = QRect(infoBg.right() - 28, infoBg.top() + 4, 22, 22);
    Codicon::draw(p, m_asset.isFavorite ? "star" : "star-empty", m_favStarRect,
                  m_asset.isFavorite ? Color::FAVORITE_ON : Color::FAVORITE_OFF, 16);
    return imgRect.bottom();
}

int DetailPanel::drawAssetSummary(QPainter &p, int y)
{
    const int rowH = 112;
    QRect summaryRect(0, y, width(), rowH);
    p.fillRect(summaryRect, Color::BG_DARK);

    QFileInfo fi(m_asset.filePath);
    QString displayName = fi.completeBaseName().isEmpty() ? m_asset.fileName : fi.completeBaseName();

    p.setFont(m_fontTitle);
    p.setPen(Color::TEXT_PRIMARY);
    QRect titleRect(16, y + 10, width() - 32, 20);
    p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(displayName, Qt::ElideRight, titleRect.width()));

    p.setFont(m_fontMeta);
    p.setPen(Color::TEXT_SECONDARY);
    QRect metaRect(16, y + 31, width() - 32, 18);
    p.drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(detailMetaLine(m_asset), Qt::ElideRight, metaRect.width()));

    int chipX = 16;
    const int chipY = y + 48;
    auto drawChip = [&](const QString &text, const QColor &color) {
        int chipW = qMin(width() - chipX - 16, p.fontMetrics().horizontalAdvance(text) + 18);
        if (chipW <= 24) return;
        QRect chipRect(chipX, chipY, chipW, 20);
        QColor bg = color;
        bg.setAlpha(44);
        p.setBrush(bg);
        p.setPen(QPen(color, 1));
        p.drawRoundedRect(chipRect, Visual::RadiusSmall, Visual::RadiusSmall);
        p.setBrush(Qt::NoBrush);
        p.setPen(color);
        p.drawText(chipRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(text, Qt::ElideRight, chipRect.width() - 16));
        chipX += chipW + 6;
    };

    if (!m_metadata.source.isEmpty())
        drawChip(UIUtils::displayNameForSource(m_metadata.source), Color::ACCENT);
    drawChip(m_metadata.prompt.isEmpty() ? tr("无 Prompt") : tr("Prompt"),
             m_metadata.prompt.isEmpty() ? Color::TEXT_SECONDARY : QColor(0x3d, 0xa3, 0x5d));
    if (!m_tags.isEmpty())
        drawChip(tr("%1 个标签").arg(m_tags.size()), QColor(0xa9, 0x70, 0xff));

    const int actionsY = y + 74;
    const int actionH = 26;
    m_copyFileNameRect = QRect(16, actionsY, 88, actionH);
    m_openFolderRect = QRect(110, actionsY, 92, actionH);
    m_openPreviewRect = QRect(208, actionsY, 92, actionH);
    drawSummaryAction(p, m_copyFileNameRect, "copy", tr("文件名"), m_copyFileNameHovered, !m_asset.fileName.isEmpty());
    drawSummaryAction(p, m_openFolderRect, "folder-opened", tr("文件夹"), m_openFolderHovered, !m_asset.filePath.isEmpty());
    drawSummaryAction(p, m_openPreviewRect, "open-preview", tr("查看大图"), m_openPreviewHovered, !m_asset.id.isEmpty());

    return y + rowH;
}

void DetailPanel::drawSummaryAction(QPainter &p, const QRect &rect, const QString &icon,
                                    const QString &text, bool hovered, bool enabled) const
{
    p.setBrush(hovered ? Color::BG_BUTTON_HOVER : Color::BG_BUTTON_SUBTLE);
    p.setPen(QPen(hovered ? Color::BORDER_SUBTLE : Color::BORDER_FAINT, 1));
    p.drawRoundedRect(rect, Visual::RadiusSmall, Visual::RadiusSmall);
    p.setBrush(Qt::NoBrush);

    QColor textColor = enabled ? (hovered ? Color::TEXT_PRIMARY : Color::TEXT_SECONDARY)
                               : Color::TEXT_DISABLED;
    Codicon::draw(p, icon, QRect(rect.left() + 8, rect.top(), 14, rect.height()), textColor, 12);
    p.setFont(m_fontCaption);
    p.setPen(textColor);
    p.drawText(rect.adjusted(26, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
}

int DetailPanel::drawFileInfo(QPainter &p, int y)
{
    p.setFont(m_fontControl);
    m_fileInfoHeaderRect = QRect(0, y - 18, width(), 24);
    if (m_fileInfoHeaderHovered)
        p.fillRect(m_fileInfoHeaderRect, Color::BG_HOVER);
    p.setPen(Color::TEXT_SECONDARY);
    Codicon::draw(p, m_fileInfoExpanded ? "chevron-down" : "chevron-right",
                  QRect(14, y - 18, 16, 22), Color::TEXT_SECONDARY, 12);
    p.setFont(m_fontControl);
    p.drawText(36, y, tr("文件信息"));
    if (!m_fileInfoExpanded) return y + 8;
    y += 24;

    QFileInfo fi(m_asset.filePath);
    drawField(p, 16, y, tr("名称"), m_asset.fileName, Visual::DetailFieldLabelWidth);
    drawField(p, 16, y, tr("大小"), QString::number(m_asset.fileSize / 1024) + " KB", Visual::DetailFieldLabelWidth);
    drawField(p, 16, y, tr("尺寸"), QStringLiteral("%1 x %2").arg(m_asset.width).arg(m_asset.height), Visual::DetailFieldLabelWidth);
    drawField(p, 16, y, tr("格式"), m_asset.format.toUpper(), Visual::DetailFieldLabelWidth);
    drawField(p, 16, y, tr("路径"), fi.absolutePath(), Visual::DetailFieldLabelWidth);
    drawField(p, 16, y, tr("修改时间"), fi.lastModified().toString("yyyy-MM-dd hh:mm"), Visual::DetailFieldLabelWidth);
    return y + 8;
}

int DetailPanel::drawMetadataSection(QPainter &p, int y)
{
    if (m_metadata.source.isEmpty())
        return y + 8;

    p.setFont(m_fontControl);
    m_metadataHeaderRect = QRect(0, y - 18, width(), 24);
    if (m_metadataHeaderHovered)
        p.fillRect(m_metadataHeaderRect, Color::BG_HOVER);
    p.setPen(Color::TEXT_SECONDARY);
    Codicon::draw(p, m_metadataExpanded ? "chevron-down" : "chevron-right",
                  QRect(14, y - 18, 16, 22), Color::TEXT_SECONDARY, 12);
    p.setFont(m_fontControl);
    p.drawText(36, y, tr("AI 元数据"));

    if (!m_metadata.prompt.isEmpty()) {
        QRect quickCopy(width() - 174, y - 19, 76, 22);
        p.setBrush(m_copyPromptHovered ? Color::BG_BUTTON_HOVER : Color::BG_BUTTON_SUBTLE);
        p.setPen(QPen(Color::BORDER_FAINT, 1));
        p.drawRoundedRect(quickCopy, Visual::RadiusSmall, Visual::RadiusSmall);
        p.setBrush(Qt::NoBrush);
        Codicon::draw(p, "copy", QRect(quickCopy.left() + 8, quickCopy.top(), 14, quickCopy.height()),
                      Color::TEXT_SECONDARY, 11);
        p.setFont(m_fontCaption);
        p.setPen(Color::TEXT_PRIMARY);
        p.drawText(quickCopy.adjusted(26, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, tr("复制"));
        m_copyPromptRect = quickCopy;
    }

    if (!m_metadataExpanded) return y + 8;
    y += 24;

    drawField(p, 16, y, tr("来源"), UIUtils::displayNameForSource(m_metadata.source), Visual::DetailFieldLabelWidth);
    if (!m_metadata.modelName.isEmpty())
        drawField(p, 16, y, "Model", m_metadata.modelName, Visual::DetailFieldLabelWidth);
    if (m_metadata.seed > 0)
        drawField(p, 16, y, "Seed", QString::number(m_metadata.seed), Visual::DetailFieldLabelWidth);

    m_promptHeaderRect = QRect(0, y - 18, width(), 24);
    if (m_promptHeaderHovered)
        p.fillRect(m_promptHeaderRect, Color::BG_HOVER);
    p.setPen(Color::TEXT_SECONDARY);
    Codicon::draw(p, m_promptExpanded ? "chevron-down" : "chevron-right",
                  QRect(18, y - 18, 16, 22), Color::TEXT_SECONDARY, 12);
    p.setFont(m_fontBody);
    p.drawText(40, y, "Prompt");

    QString promptText = m_metadata.prompt.isEmpty() ? tr("暂无 Prompt") : m_metadata.prompt;
    const bool hasPrompt = !m_metadata.prompt.isEmpty();
    const int promptTop = y + 8;
    int promptBoxHeight = 0;

    if (m_promptExpanded) {
        if (hasPrompt) {
            const int textWidth = width() - 52;
            const int promptH = p.fontMetrics().boundingRect(QRect(0, 0, textWidth, 600),
                                                             Qt::TextWordWrap, promptText).height();
            promptBoxHeight = qBound(72, promptH + 20, Visual::DetailPromptHeight);
        } else {
            promptBoxHeight = 52;
        }

        QRect promptRect(16, promptTop, width() - 32, promptBoxHeight);
        m_promptContentRect = promptRect;
        p.setBrush(Color::BG_CARD_SUBTLE);
        p.setPen(QPen(Color::BORDER_FAINT, 1));
        p.drawRoundedRect(promptRect, Visual::RadiusSmall, Visual::RadiusSmall);
        p.setBrush(Qt::NoBrush);

        QRect textRect = promptRect.adjusted(8, 8, -8, -8);
        p.setPen(hasPrompt ? Color::TEXT_PRIMARY : Color::TEXT_SECONDARY);
        if (hasPrompt) {
            QTextOption opt;
            opt.setWrapMode(QTextOption::WordWrap);
            p.drawText(textRect, promptText, opt);
        } else {
            p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, promptText);
        }
        y = promptRect.bottom() + 14;
    } else {
        QRect promptRect(16, promptTop, width() - 32, 22);
        m_promptContentRect = promptRect;
        p.setPen(hasPrompt ? Color::TEXT_PRIMARY : Color::TEXT_SECONDARY);
        p.drawText(promptRect, Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(promptText, Qt::ElideRight, promptRect.width()));
        y += 24;
    }

    if (!m_metadata.negativePrompt.isEmpty())
        drawField(p, 16, y, "Negative", m_metadata.negativePrompt, Visual::DetailFieldLabelWidth);
    if (m_metadata.steps > 0)
        drawField(p, 16, y, "Steps", QString::number(m_metadata.steps), Visual::DetailFieldLabelWidth);
    if (m_metadata.cfgScale > 0.0)
        drawField(p, 16, y, "CFG", QString::number(m_metadata.cfgScale, 'f', 1), Visual::DetailFieldLabelWidth);
    if (!m_metadata.sampler.isEmpty())
        drawField(p, 16, y, "Sampler", m_metadata.sampler, Visual::DetailFieldLabelWidth);
    return y + 8;
}

int DetailPanel::drawTagsSection(QPainter &p, int y)
{
    p.setFont(m_fontControl);
    m_tagsHeaderRect = QRect(0, y - 18, width(), 24);
    if (m_tagsHeaderHovered)
        p.fillRect(m_tagsHeaderRect, Color::BG_HOVER);
    p.setPen(Color::TEXT_SECONDARY);
    Codicon::draw(p, m_tagsExpanded ? "chevron-down" : "chevron-right",
                  QRect(14, y - 18, 16, 22), Color::TEXT_SECONDARY, 12);
    p.setFont(m_fontControl);
    p.drawText(36, y, tr("标签"));
    if (!m_tagsExpanded) return y + 8;
    y += 24;

    p.setFont(m_fontMeta);

    m_tagRects.clear();
    m_addTagRect = {};
    int tagX = 16;
    if (m_tags.isEmpty()) {
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(QRect(tagX, y - 2, width() - 32, 22), Qt::AlignLeft | Qt::AlignVCenter, tr("暂无标签"));
        y += 28;
    }

    for (const auto &tag : qAsConst(m_tags)) {
        int tw = p.fontMetrics().horizontalAdvance(tag.name) + 28;
        if (tagX + tw > width() - 16) {
            tagX = 16;
            y += 26;
        }

        QRect tagBg(tagX, y - 2, tw, 22);
        m_tagRects.append({tag.id, tagBg});
        QColor bg = tag.color;
        bg.setAlpha(34);
        p.setBrush(bg);
        p.setPen(QPen(tag.color, 1));
        p.drawRoundedRect(tagBg, Visual::RadiusMedium, Visual::RadiusMedium);
        p.setPen(tag.color);
        p.drawText(tagBg.adjusted(7, 0, -16, 0), Qt::AlignVCenter, tag.name);
        Codicon::draw(p, "close", QRect(tagBg.right() - 18, tagBg.top() + 2, 14, 18), Color::TEXT_PRIMARY, 10);
        tagX += tw + 6;
    }

    QString addText = tr("添加标签");
    int addW = p.fontMetrics().horizontalAdvance(addText) + 34;
    if (tagX + addW > width() - 16) {
        tagX = 16;
        y += 26;
    }
    m_addTagRect = QRect(tagX, y - 2, addW, 22);
    p.setBrush(m_addTagHovered ? Color::BG_BUTTON_HOVER : Color::BG_DARK);
    p.setPen(QPen(m_addTagHovered ? Color::TEXT_PRIMARY : Color::BORDER_SUBTLE, 1));
    p.drawRoundedRect(m_addTagRect, Visual::RadiusMedium, Visual::RadiusMedium);
    QColor addColor = m_addTagHovered ? Color::TEXT_PRIMARY : Color::TEXT_SECONDARY;
    Codicon::draw(p, "add", QRect(m_addTagRect.left() + 6, m_addTagRect.top(), 14, m_addTagRect.height()), addColor, 11);
    p.setPen(addColor);
    p.drawText(m_addTagRect.adjusted(24, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, addText);
    return y + 30;
}

void DetailPanel::drawField(QPainter &p, int x, int &y, const QString &label, const QString &value, int labelW) const
{
    p.setFont(m_fontBody);
    p.setPen(Color::TEXT_SECONDARY);
    p.drawText(x, y, label);

    QString display = value.isEmpty() ? QStringLiteral("--") : value;
    QRect valueRect(x + labelW, y - 14, width() - x - labelW - 16, 20);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(valueRect, Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(display, Qt::ElideRight, valueRect.width()));
    y += 24;
}

void DetailPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (m_closeBtnRect.contains(event->pos()) && !m_asset.id.isEmpty()) {
        setVisible(false);
        return;
    }
    if (m_favStarRect.contains(event->pos()) && !m_asset.id.isEmpty()) {
        m_asset.isFavorite = !m_asset.isFavorite;
        emit favoriteToggled(m_asset.id, m_asset.isFavorite);
        update();
        return;
    }
    if (m_copyPromptRect.contains(event->pos()) && !m_metadata.prompt.isEmpty()) {
        QApplication::clipboard()->setText(m_metadata.prompt);
        ToastNotification::show(this, tr("已复制 Prompt"));
        return;
    }
    if (m_promptContentRect.contains(event->pos()) && !m_asset.id.isEmpty() && !m_isEditingPrompt) {
        startPromptEdit();
        return;
    }
    if (m_copyFileNameRect.contains(event->pos()) && !m_asset.fileName.isEmpty()) {
        QApplication::clipboard()->setText(m_asset.fileName);
        ToastNotification::show(this, tr("已复制文件名"));
        return;
    }
    if (m_openFolderRect.contains(event->pos()) && !m_asset.filePath.isEmpty()) {
        QFileInfo info(m_asset.filePath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
        ToastNotification::show(this, tr("已打开所在文件夹"));
        return;
    }
    if (m_openPreviewRect.contains(event->pos()) && !m_asset.id.isEmpty()) {
        emit previewRequested(m_asset.id);
        return;
    }
    if (m_addTagRect.contains(event->pos()) && !m_asset.id.isEmpty()) {
        emit tagAddRequested(m_asset.id);
        return;
    }
    for (const auto &pair : qAsConst(m_tagRects)) {
        QRect xRect(pair.second.right() - 16, pair.second.top() + 3, 12, 16);
        if (xRect.contains(event->pos())) {
            emit tagRemoved(m_asset.id, pair.first);
            return;
        }
    }
    if (!m_asset.id.isEmpty()) {
        if (m_fileInfoHeaderRect.contains(event->pos())) { m_fileInfoExpanded = !m_fileInfoExpanded; update(); return; }
        if (m_metadataHeaderRect.contains(event->pos())) { m_metadataExpanded = !m_metadataExpanded; update(); return; }
        if (m_tagsHeaderRect.contains(event->pos())) { m_tagsExpanded = !m_tagsExpanded; update(); return; }
    }
    if (m_promptHeaderRect.contains(event->pos()) && !m_asset.id.isEmpty()) {
        m_promptExpanded = !m_promptExpanded;
        update();
        return;
    }
    if (imageArea().contains(event->pos()) && m_zoom > 1.05) {
        m_isPanning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void DetailPanel::mouseMoveEvent(QMouseEvent *event)
{
    bool oldAddHover = m_addTagHovered;
    bool oldCopyHover = m_copyPromptHovered;
    bool oldCloseHover = m_closeBtnHovered;
    bool oldCopyFileNameHover = m_copyFileNameHovered;
    bool oldOpenFolderHover = m_openFolderHovered;
    bool oldOpenPreviewHover = m_openPreviewHovered;
    bool oldPromptHover = m_promptHeaderHovered;
    bool oldFileInfoHover = m_fileInfoHeaderHovered;
    bool oldMetaHover = m_metadataHeaderHovered;
    bool oldTagHover = m_tagsHeaderHovered;
    m_addTagHovered = m_addTagRect.contains(event->pos()) && !m_asset.id.isEmpty();
    m_copyPromptHovered = m_copyPromptRect.contains(event->pos()) && !m_metadata.prompt.isEmpty();
    m_closeBtnHovered = m_closeBtnRect.contains(event->pos()) && !m_asset.id.isEmpty();
    m_copyFileNameHovered = m_copyFileNameRect.contains(event->pos()) && !m_asset.fileName.isEmpty();
    m_openFolderHovered = m_openFolderRect.contains(event->pos()) && !m_asset.filePath.isEmpty();
    m_openPreviewHovered = m_openPreviewRect.contains(event->pos()) && !m_asset.id.isEmpty();
    m_promptHeaderHovered = m_promptHeaderRect.contains(event->pos()) && !m_asset.id.isEmpty();
    m_fileInfoHeaderHovered = m_fileInfoHeaderRect.contains(event->pos()) && !m_asset.id.isEmpty();
    m_metadataHeaderHovered = m_metadataHeaderRect.contains(event->pos()) && !m_asset.id.isEmpty();
    m_tagsHeaderHovered = m_tagsHeaderRect.contains(event->pos()) && !m_asset.id.isEmpty();
    if (oldAddHover != m_addTagHovered) update(m_addTagRect);
    if (oldCopyHover != m_copyPromptHovered) update(m_copyPromptRect);
    if (oldCloseHover != m_closeBtnHovered) update(m_closeBtnRect);
    if (oldCopyFileNameHover != m_copyFileNameHovered) update(m_copyFileNameRect);
    if (oldOpenFolderHover != m_openFolderHovered) update(m_openFolderRect);
    if (oldOpenPreviewHover != m_openPreviewHovered) update(m_openPreviewRect);
    if (oldPromptHover != m_promptHeaderHovered) update(m_promptHeaderRect);
    if (oldFileInfoHover != m_fileInfoHeaderHovered) update(m_fileInfoHeaderRect);
    if (oldMetaHover != m_metadataHeaderHovered) update(m_metadataHeaderRect);
    if (oldTagHover != m_tagsHeaderHovered) update(m_tagsHeaderRect);

    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_panOffset += delta;
        m_lastPanPos = event->pos();
        update(imageArea());
    }
}

void DetailPanel::mouseReleaseEvent(QMouseEvent *)
{
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
}

void DetailPanel::wheelEvent(QWheelEvent *event)
{
    if (m_isEditingPrompt) {
        finishPromptEdit(true);
        event->accept();
        return;
    }
    QPoint pixelDelta = event->pixelDelta();
    QPoint angleDelta = event->angleDelta();
    if (imageArea().contains(event->pos())) {
        double factor = angleDelta.y() > 0 ? 1.1 : 0.9;
        m_zoom = qBound(0.1, m_zoom * factor, 10.0);
        update();
    } else if (m_maxScrollOffset > 0) {
        int delta = pixelDelta.isNull() ? angleDelta.y() / 8 : pixelDelta.y();
        m_scrollOffset -= delta;
        m_scrollOffset = qBound(0, m_scrollOffset, m_maxScrollOffset);
        update();
    }
}

void DetailPanel::startPromptEdit()
{
    m_isEditingPrompt = true;
    if (!m_promptExpanded) {
        m_promptExpanded = true;
        update();
    }
    auto promptFont = [this]() {
            return m_fontBody;
    };
    QFontMetrics fm(promptFont());
    const int textW = width() - 52;
    const int textH = fm.boundingRect(QRect(0, 0, textW, 600),
                                      Qt::TextWordWrap, m_metadata.prompt).height();
    QRect editorRect = m_promptContentRect;
    editorRect.setHeight(qBound(52, textH + 20, Visual::DetailPromptHeight));
    m_promptEditor->setPlainText(m_metadata.prompt);
    m_promptEditor->setGeometry(editorRect);
    m_promptEditor->setVisible(true);
    m_promptEditor->setFocus();
    m_promptEditor->selectAll();
    m_promptEditor->setToolTip(tr("Ctrl+Enter 保存，Esc 取消"));

    QTimer::singleShot(0, this, [this, promptFont]() {
        QRect r = m_promptContentRect;
        if (!r.isValid()) return;
        QFontMetrics fm2(promptFont());
        const int textH2 = fm2.boundingRect(QRect(0, 0, width() - 52, 600),
                                            Qt::TextWordWrap, m_metadata.prompt).height();
        r.setHeight(qBound(52, textH2 + 20, Visual::DetailPromptHeight));
        m_promptEditor->setGeometry(r);
    });
}

void DetailPanel::finishPromptEdit(bool accepted)
{
    if (!m_isEditingPrompt) return;
    m_isEditingPrompt = false;
    m_promptEditor->setVisible(false);

    if (accepted) {
        QString newPrompt = m_promptEditor->toPlainText().trimmed();
        if (newPrompt != m_metadata.prompt) {
            m_metadata.prompt = newPrompt;
            emit promptSaved(m_asset.id, newPrompt);
        }
    }
    setFocus();
    update();
}

bool DetailPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_promptEditor && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return && (key->modifiers() & Qt::ControlModifier)) {
            finishPromptEdit(true);
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            finishPromptEdit(false);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DetailPanel::hideEvent(QHideEvent *event)
{
    finishPromptEdit(false);
    QWidget::hideEvent(event);
}

void DetailPanel::clampScrollOffset()
{
    m_scrollOffset = qBound(0, m_scrollOffset, m_maxScrollOffset);
}

void DetailPanel::drawScrollIndicator(QPainter &p) const
{
    if (m_maxScrollOffset <= 0) return;
    int indicatorH = qMax(30, height() * height() / (height() + m_maxScrollOffset));
    int indicatorY = (height() - indicatorH) * m_scrollOffset / m_maxScrollOffset;
    QRect indicatorRect(width() - 4, indicatorY, 3, indicatorH);
    p.setBrush(Color::SCROLLBAR);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(indicatorRect, 2, 2);
}

void DetailPanel::resizeEvent(QResizeEvent *)
{
    if (m_isEditingPrompt) {
        finishPromptEdit(true);
    }
    clampScrollOffset();
    update();
}

void DetailPanel::leaveEvent(QEvent *)
{
    if (m_addTagHovered || m_copyPromptHovered || m_closeBtnHovered
        || m_copyFileNameHovered || m_openFolderHovered || m_openPreviewHovered
        || m_promptHeaderHovered || m_fileInfoHeaderHovered
        || m_metadataHeaderHovered || m_tagsHeaderHovered) {
        m_addTagHovered = false;
        m_copyPromptHovered = false;
        m_closeBtnHovered = false;
        m_copyFileNameHovered = false;
        m_openFolderHovered = false;
        m_openPreviewHovered = false;
        m_promptHeaderHovered = false;
        m_fileInfoHeaderHovered = false;
        m_metadataHeaderHovered = false;
        m_tagsHeaderHovered = false;
        update();
    }
}
