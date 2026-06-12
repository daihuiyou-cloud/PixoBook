#include "DetailPanel.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFileInfo>
#include "ui/Codicon.h"

DetailPanel::DetailPanel(IImageCache *cache, QWidget *parent)
    : QWidget(parent), m_cache(cache)
{
    setMouseTracking(true);
    setMinimumWidth(300);
}

void DetailPanel::showAsset(const Asset &asset, const Metadata &metadata, const QVector<Tag> &tags)
{
    m_asset = asset;
    m_metadata = metadata;
    m_tags = tags;
    m_zoom = 1.0;
    m_panOffset = {};
    m_promptExpanded = true;

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
    update();
}

QRect DetailPanel::imageArea() const
{
    return QRect(0, 0, width(), qMin(width() * 3 / 4, height() / 3));
}

QRect DetailPanel::metadataArea() const
{
    int imgBottom = imageArea().bottom();
    return QRect(16, imgBottom + 8, width() - 32, height() - imgBottom - 16);
}

void DetailPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), QColor(0x25, 0x25, 0x26));

    if (m_asset.id.isEmpty()) {
        p.setPen(QColor(0x96, 0x96, 0x96));
        p.drawText(rect(), Qt::AlignCenter,
                   QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe4\xb8\x80\xe5\xbc\xa0\xe5\x9b\xbe\xe7\x89\x87"
                                       "\xe6\x9f\xa5\xe7\x9c\x8b\xe8\xaf\xa6\xe6\x83\x85"));
        return;
    }

    // Close button (top right)
    m_closeBtnRect = QRect(width() - 28, 8, 20, 20);
    Codicon::draw(p, "close", m_closeBtnRect, QColor(0x96, 0x96, 0x96), 14);

    int y = drawImage(p);
    drawSectionDivider(p, y);
    y += 2;
    y = drawFileInfo(p, y + 16);
    drawSectionDivider(p, y);
    y += 2;
    y = drawMetadataSection(p, y + 16);
    drawSectionDivider(p, y);
    y += 2;
    drawTagsSection(p, y + 16);
}

void DetailPanel::drawSectionDivider(QPainter &p, int y)
{
    p.setPen(QColor(0x3c, 0x3c, 0x3c));
    p.drawLine(0, y, width(), y);
}

int DetailPanel::drawImage(QPainter &p)
{
    QRect imgRect = imageArea();
    p.fillRect(imgRect, QColor(0x1e, 0x1e, 0x1e));

    if (m_fullImage.isNull()) {
        p.setPen(QColor(0x96, 0x96, 0x96));
        p.drawText(imgRect, Qt::AlignCenter,
                   QString::fromUtf8("\xe6\x97\xa0\xe6\xb3\x95\xe5\x8a\xa0\xe8\xbd\xbd\xe5\x9b\xbe\xe7\x89\x87"));
        return imgRect.bottom();
    }

    double scale = m_zoom * qMin(
        (double)imgRect.width() / m_fullImage.width(),
        (double)imgRect.height() / m_fullImage.height()
    );

    int drawW = (int)(m_fullImage.width() * scale);
    int drawH = (int)(m_fullImage.height() * scale);

    int imgX = imgRect.center().x() - drawW / 2 + m_panOffset.x();
    int imgY = imgRect.center().y() - drawH / 2 + m_panOffset.y();

    p.drawPixmap(imgX, imgY, drawW, drawH, m_fullImage);

    // Overlay bar
    QRect infoBg(imgRect.left(), imgRect.bottom() - 28, imgRect.width(), 28);
    p.fillRect(infoBg, QColor(0, 0, 0, 160));
    p.setPen(QColor(0xcc, 0xcc, 0xcc));
    p.drawText(infoBg.adjusted(8, 0, 28, 0), Qt::AlignVCenter,
               QString("%1 x %2  |  %3%")
                   .arg(m_asset.width).arg(m_asset.height)
                   .arg((int)(m_zoom * 100)));

    // Favorite star button in overlay bar
    m_favStarRect = QRect(infoBg.right() - 24, infoBg.top() + 4, 20, 20);
    QColor starClr = m_asset.isFavorite ? QColor(0xff, 0xcc, 0x00) : QColor(0x60, 0x60, 0x60);
    Codicon::draw(p, "star", m_favStarRect, starClr, 16);

    return imgRect.bottom();
}

int DetailPanel::drawFileInfo(QPainter &p, int y)
{
    QFont hf;
    hf.setBold(true);
    p.setFont(hf);
    m_fileInfoHeaderRect = QRect(0, y - 18, width(), 22);
    p.setPen(QColor(0x96, 0x96, 0x96));
    p.drawText(10, y, QString(m_fileInfoExpanded ? QChar(0x25BC) : QChar(0x25B6))
               + "  " + QString::fromUtf8("\xe6\x96\x87\xe4\xbb\xb6\xe4\xbf\xa1\xe6\x81\xaf"));
    if (!m_fileInfoExpanded)
        return y;
    y += 22;

    QFileInfo fi(m_asset.filePath);
    drawField(p, 16, y, QString::fromUtf8("\xe5\x90\x8d\xe7\xa7\xb0"), m_asset.fileName);
    drawField(p, 16, y, QString::fromUtf8("\xe5\xa4\xa7\xe5\xb0\x8f"),
              QString::number(m_asset.fileSize / 1024) + " KB");
    drawField(p, 16, y, QString::fromUtf8("\xe5\xb0\xba\xe5\xaf\xb8"),
              QString("%1 x %2").arg(m_asset.width).arg(m_asset.height));
    drawField(p, 16, y, QString::fromUtf8("\xe6\xa0\xbc\xe5\xbc\x8f"), m_asset.format.toUpper());
    drawField(p, 16, y, QString::fromUtf8("\xe4\xbf\xae\xe6\x94\xb9\xe6\x97\xb6\xe9\x97\xb4"),
              fi.lastModified().toString("yyyy-MM-dd hh:mm"));

    return y;
}

int DetailPanel::drawMetadataSection(QPainter &p, int y)
{
    if (m_metadata.source.isEmpty())
        return y + 4;

    QFont hf;
    hf.setBold(true);
    p.setFont(hf);
    m_metadataHeaderRect = QRect(0, y - 18, width(), 22);
    p.setPen(QColor(0x96, 0x96, 0x96));
    p.drawText(10, y, QString(m_metadataExpanded ? QChar(0x25BC) : QChar(0x25B6))
               + "  " + QString::fromUtf8("AI \xe5\x85\x83\xe6\x95\xb0\xe6\x8d\xae"));
    if (!m_metadataExpanded)
        return y + 4;
    y += 22;

    QString sourceDisplay = m_metadata.source;
    if (sourceDisplay == "stable-diffusion") sourceDisplay = "Stable Diffusion";
    else if (sourceDisplay == "midjourney") sourceDisplay = "Midjourney";
    else if (sourceDisplay == "dalle") sourceDisplay = "DALL-E";
    drawField(p, 16, y, QString::fromUtf8("\xe6\x9d\xa5\xe6\xba\x90"), sourceDisplay);

    // Prompt with toggle
    m_promptHeaderRect = QRect(0, y - 18, width(), 22);
    p.setPen(QColor(0x96, 0x96, 0x96));
    p.drawText(16, y, QString(m_promptExpanded ? QChar(0x25BC) : QChar(0x25B6)) + "  Prompt");
    p.setPen(QColor(0xcc, 0xcc, 0xcc));
    QString promptText = m_metadata.prompt.isEmpty()
        ? QString::fromUtf8("\xe2\x80\x94")
        : (m_promptExpanded || m_metadata.prompt.length() < 100
           ? m_metadata.prompt
           : m_metadata.prompt.left(100) + "...");
    if (m_promptExpanded) {
        p.drawText(100, y, width() - 110, 40, Qt::AlignLeft | Qt::TextWordWrap, promptText);
        int promptH = p.fontMetrics().boundingRect(QRect(0, 0, width() - 110, 100),
                                                    Qt::AlignLeft | Qt::TextWordWrap, promptText).height();
        y += qMax(20, promptH + 4);
    } else {
        p.drawText(100, y, width() - 110, 40, Qt::AlignLeft | Qt::TextWordWrap, promptText);
        y += 22;
    }

    if (!m_metadata.negativePrompt.isEmpty()) {
        drawField(p, 16, y, "Negative", m_metadata.negativePrompt.length() > 80
                  ? m_metadata.negativePrompt.left(80) + "..."
                  : m_metadata.negativePrompt);
    }
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

    return y;
}

int DetailPanel::drawTagsSection(QPainter &p, int y)
{
    if (m_tags.isEmpty())
        return y;

    QFont hf;
    hf.setBold(true);
    p.setFont(hf);
    m_tagsHeaderRect = QRect(0, y - 18, width(), 22);
    p.setPen(QColor(0x96, 0x96, 0x96));
    p.drawText(16, y, QString(m_tagsExpanded ? QChar(0x25BC) : QChar(0x25B6))
               + "  " + QString::fromUtf8("\xe6\xa0\x87\xe7\xad\xbe"));
    if (!m_tagsExpanded)
        return y;
    y += 22;

    QFont nf;
    nf.setPointSize(9);
    p.setFont(nf);

    m_tagRects.clear();
    m_addTagRect = {};
    int tagX = 16;
    for (const auto &tag : m_tags) {
        int tw = p.fontMetrics().horizontalAdvance(tag.name) + 26;
        if (tagX + tw > width() - 16) { tagX = 16; y += 24; }

        QRect tagBg(tagX, y - 2, tw, 20);
        m_tagRects.append({tag.id, tagBg});
        QColor c = tag.color;
        c.setAlpha(30);
        p.setBrush(c);
        p.setPen(QPen(tag.color, 1));
        p.drawRoundedRect(tagBg, 3, 3);

        p.setPen(tag.color);
        p.drawText(tagBg.adjusted(4, 0, -16, 0), Qt::AlignCenter, tag.name);

        QRect xRect(tagBg.right() - 14, tagBg.top() + 2, 12, 16);
        Codicon::draw(p, "close", xRect, QColor(0xcc, 0xcc, 0xcc), 10);
        tagX += tw + 6;
    }

    // "+" add tag chip
    int addTw = 24;
    if (tagX + addTw > width() - 16) { tagX = 16; y += 24; }
    m_addTagRect = QRect(tagX, y - 2, addTw, 20);
    p.setBrush(QColor(0x3c, 0x3c, 0x3c));
    p.setPen(QPen(QColor(0x96, 0x96, 0x96), 1));
    p.drawRoundedRect(m_addTagRect, 4, 4);
    Codicon::draw(p, "add", m_addTagRect, QColor(0x96, 0x96, 0x96), 12);

    y += 24;

    return y;
}

void DetailPanel::drawField(QPainter &p, int x, int &y, const QString &label, const QString &value, int labelW)
{
    p.setPen(QColor(0x96, 0x96, 0x96));
    p.drawText(x, y, label);

    p.setPen(QColor(0xcc, 0xcc, 0xcc));
    p.drawText(x + labelW, y, value.isEmpty() ? QString::fromUtf8("\xe2\x80\x94") : value);

    y += 18;
}

void DetailPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    // Close button
    if (m_closeBtnRect.contains(event->pos()) && !m_asset.id.isEmpty()) {
        setVisible(false);
        return;
    }

    // Favorite star toggle
    if (m_favStarRect.contains(event->pos()) && !m_asset.id.isEmpty()) {
        m_asset.isFavorite = !m_asset.isFavorite;
        emit favoriteToggled(m_asset.id, m_asset.isFavorite);
        update();
        return;
    }

    // Add tag "+" button
    if (m_addTagRect.contains(event->pos()) && !m_asset.id.isEmpty()) {
        emit tagAddRequested(m_asset.id);
        return;
    }

    // Tag remove × button
    for (const auto &pair : m_tagRects) {
        QRect xRect(pair.second.right() - 14, pair.second.top() + 2, 12, 16);
        if (xRect.contains(event->pos())) {
            emit tagRemoved(m_asset.id, pair.first);
            return;
        }
    }

    // Section header toggles
    if (!m_asset.id.isEmpty()) {
        if (m_fileInfoHeaderRect.contains(event->pos())) {
            m_fileInfoExpanded = !m_fileInfoExpanded;
            update(); return;
        }
        if (m_metadataHeaderRect.contains(event->pos())) {
            m_metadataExpanded = !m_metadataExpanded;
            update(); return;
        }
        if (m_tagsHeaderRect.contains(event->pos())) {
            m_tagsExpanded = !m_tagsExpanded;
            update(); return;
        }
    }

    // Prompt header toggle
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
    }
}

void DetailPanel::resizeEvent(QResizeEvent *)
{
    update();
}
