#include "DetailPanel.h"
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QTextOption>
#include <QWheelEvent>
#include "ui/Codicon.h"
#include "ui/ColorConstants.h"
#include "ui/VisualConstants.h"

DetailPanel::DetailPanel(IImageCache *cache, QWidget *parent)
    : QWidget(parent), m_cache(cache)
{
    setMouseTracking(true);
    setMinimumWidth(340);
}

void DetailPanel::showAsset(const Asset &asset, const Metadata &metadata, const QVector<Tag> &tags)
{
    m_asset = asset;
    m_metadata = metadata;
    m_tags = tags;
    m_zoom = 1.0;
    m_panOffset = {};
    m_scrollOffset = 0;

    m_fullImage = QPixmap(asset.filePath);
    if (m_fullImage.isNull())
        m_fullImage = QPixmap();
    update();
}

void DetailPanel::clear()
{
    m_asset = {};
    m_metadata = {};
    m_tags.clear();
    m_fullImage = QPixmap();
    m_closeBtnRect = QRect();
    m_copyPromptRect = QRect();
    m_scrollOffset = 0;
    m_maxScrollOffset = 0;
    update();
}

QRect DetailPanel::imageArea() const
{
    return QRect(0, 0, width(), qBound(220, height() / 3, 340));
}

void DetailPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Color::BG_DARK);

    if (m_asset.id.isEmpty()) {
        QFont f = p.font();
        f.setPixelSize(Visual::FontBody);
        p.setFont(f);
        Codicon::draw(p, "info", QRect(width() / 2 - 18, height() / 2 - 48, 36, 36),
                      Color::TEXT_SECONDARY, 28);
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(rect().adjusted(24, 18, -24, 0), Qt::AlignCenter,
                   QStringLiteral("选择一张素材查看详情"));
        return;
    }

    m_closeBtnRect = QRect(width() - 30, 8, 22, 22);
    Codicon::draw(p, "close", m_closeBtnRect, Color::TEXT_SECONDARY, 14);

    int imageBottom = drawImage(p);
    drawSectionDivider(p, imageBottom);

    int scrollTop = imageBottom + 1;
    int scrollHeight = height() - scrollTop;

    p.save();
    p.setClipRect(0, scrollTop, width(), scrollHeight);

    int contentY = scrollTop + 14 - m_scrollOffset;
    int y = drawFileInfo(p, contentY);
    drawSectionDivider(p, y);
    y = drawMetadataSection(p, y + 18);
    drawSectionDivider(p, y);
    y = drawTagsSection(p, y + 18);

    p.restore();

    int totalContent = y - scrollTop + 16;
    m_maxScrollOffset = qMax(0, totalContent - scrollHeight);
    m_scrollOffset = qBound(0, m_scrollOffset, m_maxScrollOffset);
}

void DetailPanel::drawSectionDivider(QPainter &p, int y)
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
        p.drawText(imgRect, Qt::AlignCenter, QStringLiteral("无法加载图片"));
        return imgRect.bottom();
    }

    double scale = m_zoom * qMin((double)imgRect.width() / m_fullImage.width(),
                                (double)imgRect.height() / m_fullImage.height());
    int drawW = (int)(m_fullImage.width() * scale);
    int drawH = (int)(m_fullImage.height() * scale);
    int imgX = imgRect.center().x() - drawW / 2 + m_panOffset.x();
    int imgY = imgRect.center().y() - drawH / 2 + m_panOffset.y();
    p.drawPixmap(imgX, imgY, drawW, drawH, m_fullImage);

    QRect infoBg(imgRect.left(), imgRect.bottom() - 30, imgRect.width(), 30);
    p.fillRect(infoBg, Color::OVERLAY_LIGHT);
    QFont f = p.font();
    f.setPixelSize(Visual::FontMeta);
    p.setFont(f);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(infoBg.adjusted(10, 0, 34, 0), Qt::AlignVCenter,
               QString("%1 x %2  |  %3%").arg(m_asset.width).arg(m_asset.height).arg((int)(m_zoom * 100)));

    m_favStarRect = QRect(infoBg.right() - 28, infoBg.top() + 4, 22, 22);
    Codicon::draw(p, m_asset.isFavorite ? "star" : "star-empty", m_favStarRect,
                  m_asset.isFavorite ? Color::FAVORITE_ON : Color::FAVORITE_OFF, 16);
    return imgRect.bottom();
}

int DetailPanel::drawFileInfo(QPainter &p, int y)
{
    QFont hf = p.font();
    hf.setPixelSize(Visual::FontControl);
    hf.setBold(true);
    p.setFont(hf);
    m_fileInfoHeaderRect = QRect(0, y - 18, width(), 24);
    p.setPen(Color::TEXT_SECONDARY);
    Codicon::draw(p, m_fileInfoExpanded ? "chevron-down" : "chevron-right",
                  QRect(14, y - 18, 16, 22), Color::TEXT_SECONDARY, 12);
    p.setFont(hf);
    p.drawText(36, y, QStringLiteral("文件信息"));
    if (!m_fileInfoExpanded) return y + 8;
    y += 24;

    QFileInfo fi(m_asset.filePath);
    drawField(p, 16, y, QStringLiteral("名称"), m_asset.fileName);
    drawField(p, 16, y, QStringLiteral("大小"), QString::number(m_asset.fileSize / 1024) + " KB");
    drawField(p, 16, y, QStringLiteral("尺寸"), QString("%1 x %2").arg(m_asset.width).arg(m_asset.height));
    drawField(p, 16, y, QStringLiteral("格式"), m_asset.format.toUpper());
    drawField(p, 16, y, QStringLiteral("修改时间"), fi.lastModified().toString("yyyy-MM-dd hh:mm"));
    return y + 8;
}

int DetailPanel::drawMetadataSection(QPainter &p, int y)
{
    if (m_metadata.source.isEmpty())
        return y + 8;

    QFont hf = p.font();
    hf.setPixelSize(Visual::FontControl);
    hf.setBold(true);
    p.setFont(hf);
    m_metadataHeaderRect = QRect(0, y - 18, width(), 24);
    p.setPen(Color::TEXT_SECONDARY);
    Codicon::draw(p, m_metadataExpanded ? "chevron-down" : "chevron-right",
                  QRect(14, y - 18, 16, 22), Color::TEXT_SECONDARY, 12);
    p.setFont(hf);
    p.drawText(36, y, QStringLiteral("AI 元数据"));
    if (!m_metadataExpanded) return y + 8;
    y += 24;

    QString sourceDisplay = m_metadata.source;
    if (sourceDisplay == "stable-diffusion") sourceDisplay = "Stable Diffusion";
    else if (sourceDisplay == "midjourney") sourceDisplay = "Midjourney";
    else if (sourceDisplay == "dalle") sourceDisplay = "DALL-E";
    drawField(p, 16, y, QStringLiteral("来源"), sourceDisplay);

    m_promptHeaderRect = QRect(0, y - 18, width(), 24);
    p.setPen(Color::TEXT_SECONDARY);
    Codicon::draw(p, m_promptExpanded ? "chevron-down" : "chevron-right",
                  QRect(18, y - 18, 16, 22), Color::TEXT_SECONDARY, 12);
    QFont body = p.font();
    body.setPixelSize(Visual::FontBody);
    body.setBold(false);
    p.setFont(body);
    p.drawText(40, y, "Prompt");

    m_copyPromptRect = QRect(width() - 42, y - 18, 24, 22);
    Codicon::draw(p, "copy", m_copyPromptRect,
                  m_metadata.prompt.isEmpty() ? Color::TEXT_DISABLED : Color::TEXT_SECONDARY, 12);

    QString promptText = m_metadata.prompt.isEmpty() ? QStringLiteral("暂无 Prompt") : m_metadata.prompt;
    QRect promptRect(104, y - 14, width() - 122, m_promptExpanded ? 96 : 22);
    p.setPen(m_metadata.prompt.isEmpty() ? Color::TEXT_SECONDARY : Color::TEXT_PRIMARY);
    if (m_promptExpanded) {
        promptRect.setRight(width() - 18);
        QTextOption opt;
        opt.setWrapMode(QTextOption::WordWrap);
        p.drawText(promptRect, promptText, opt);
        int promptH = p.fontMetrics().boundingRect(QRect(0, 0, promptRect.width(), 300),
                                                   Qt::TextWordWrap, promptText).height();
        y += qBound(28, promptH + 10, 108);
    } else {
        p.drawText(promptRect, Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(promptText, Qt::ElideRight, promptRect.width()));
        y += 24;
    }

    if (!m_metadata.negativePrompt.isEmpty())
        drawField(p, 16, y, "Negative", m_metadata.negativePrompt);
    if (m_metadata.seed > 0)
        drawField(p, 16, y, "Seed", QString::number(m_metadata.seed));
    if (m_metadata.steps > 0)
        drawField(p, 16, y, "Steps", QString::number(m_metadata.steps));
    if (m_metadata.cfgScale > 0.0)
        drawField(p, 16, y, "CFG", QString::number(m_metadata.cfgScale, 'f', 1));
    if (!m_metadata.modelName.isEmpty())
        drawField(p, 16, y, "Model", m_metadata.modelName);
    if (!m_metadata.sampler.isEmpty())
        drawField(p, 16, y, "Sampler", m_metadata.sampler);
    return y + 8;
}

int DetailPanel::drawTagsSection(QPainter &p, int y)
{
    QFont hf = p.font();
    hf.setPixelSize(Visual::FontControl);
    hf.setBold(true);
    p.setFont(hf);
    m_tagsHeaderRect = QRect(0, y - 18, width(), 24);
    p.setPen(Color::TEXT_SECONDARY);
    Codicon::draw(p, m_tagsExpanded ? "chevron-down" : "chevron-right",
                  QRect(14, y - 18, 16, 22), Color::TEXT_SECONDARY, 12);
    p.setFont(hf);
    p.drawText(36, y, QStringLiteral("标签"));
    if (!m_tagsExpanded) return y + 8;
    y += 24;

    QFont nf = p.font();
    nf.setPixelSize(Visual::FontMeta);
    nf.setBold(false);
    p.setFont(nf);

    m_tagRects.clear();
    m_addTagRect = {};
    int tagX = 16;
    if (m_tags.isEmpty()) {
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(QRect(tagX, y - 2, width() - 32, 22),
                   Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("暂无标签"));
        y += 28;
    }

    for (const auto &tag : m_tags) {
        int tw = p.fontMetrics().horizontalAdvance(tag.name) + 28;
        if (tagX + tw > width() - 16) { tagX = 16; y += 26; }

        QRect tagBg(tagX, y - 2, tw, 22);
        m_tagRects.append({tag.id, tagBg});
        QColor bg = tag.color;
        bg.setAlpha(34);
        p.setBrush(bg);
        p.setPen(QPen(tag.color, 1));
        p.drawRoundedRect(tagBg, Visual::RadiusSmall, Visual::RadiusSmall);
        p.setPen(tag.color);
        p.drawText(tagBg.adjusted(7, 0, -16, 0), Qt::AlignVCenter, tag.name);
        Codicon::draw(p, "close", QRect(tagBg.right() - 16, tagBg.top() + 3, 12, 16),
                      Color::TEXT_PRIMARY, 9);
        tagX += tw + 6;
    }

    QString addText = QStringLiteral("添加标签");
    int addW = p.fontMetrics().horizontalAdvance(addText) + 34;
    if (tagX + addW > width() - 16) { tagX = 16; y += 26; }
    m_addTagRect = QRect(tagX, y - 2, addW, 22);
    p.setBrush(m_addTagHovered ? Color::BG_BUTTON_HOVER : Color::BG_DARK);
    p.setPen(QPen(m_addTagHovered ? Color::TEXT_PRIMARY : Color::BORDER_SUBTLE, 1));
    p.drawRoundedRect(m_addTagRect, Visual::RadiusSmall, Visual::RadiusSmall);
    QColor addColor = m_addTagHovered ? Color::TEXT_PRIMARY : Color::TEXT_SECONDARY;
    Codicon::draw(p, "add", QRect(m_addTagRect.left() + 6, m_addTagRect.top(), 14, m_addTagRect.height()),
                  addColor, 11);
    p.setPen(addColor);
    p.drawText(m_addTagRect.adjusted(24, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, addText);
    return y + 30;
}

void DetailPanel::drawField(QPainter &p, int x, int &y, const QString &label, const QString &value, int labelW)
{
    QFont body = p.font();
    body.setPixelSize(Visual::FontBody);
    body.setBold(false);
    p.setFont(body);
    p.setPen(Color::TEXT_SECONDARY);
    p.drawText(x, y, label);

    QString display = value.isEmpty() ? QStringLiteral("—") : value;
    QRect valueRect(x + labelW, y - 14, width() - x - labelW - 16, 20);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(valueRect, Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(display, Qt::ElideRight, valueRect.width()));
    y += 21;
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
        return;
    }
    if (m_addTagRect.contains(event->pos()) && !m_asset.id.isEmpty()) {
        emit tagAddRequested(m_asset.id);
        return;
    }
    for (const auto &pair : m_tagRects) {
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
    if (imageArea().contains(event->pos())) {
        m_isPanning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void DetailPanel::mouseMoveEvent(QMouseEvent *event)
{
    bool oldAddHover = m_addTagHovered;
    m_addTagHovered = m_addTagRect.contains(event->pos()) && !m_asset.id.isEmpty();
    if (oldAddHover != m_addTagHovered)
        update();

    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_panOffset += delta;
        m_lastPanPos = event->pos();
        update();
    }
}

void DetailPanel::mouseReleaseEvent(QMouseEvent *)
{
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
}

void DetailPanel::wheelEvent(QWheelEvent *event)
{
    if (imageArea().contains(event->pos())) {
        double factor = event->angleDelta().y() > 0 ? 1.1 : 0.9;
        m_zoom = qBound(0.1, m_zoom * factor, 10.0);
        update();
    } else if (m_maxScrollOffset > 0) {
        m_scrollOffset -= event->angleDelta().y() / 8;
        m_scrollOffset = qBound(0, m_scrollOffset, m_maxScrollOffset);
        update();
    }
}

void DetailPanel::resizeEvent(QResizeEvent *) { update(); }

void DetailPanel::leaveEvent(QEvent *)
{
    if (m_addTagHovered) {
        m_addTagHovered = false;
        update();
    }
}
