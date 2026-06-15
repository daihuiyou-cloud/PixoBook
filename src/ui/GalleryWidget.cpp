#include "GalleryWidget.h"
#include <QAction>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QUrl>
#include <QWheelEvent>
#include "ui/Codicon.h"
#include "ui/ColorConstants.h"
#include "ui/UIUtils.h"
#include "ui/VisualConstants.h"

namespace {
QString metaLine(const Asset &asset)
{
    QStringList parts;
    if (asset.width > 0 && asset.height > 0)
        parts << QString("%1 x %2").arg(asset.width).arg(asset.height);
    if (!asset.format.isEmpty())
        parts << asset.format.toUpper();
    if (asset.fileSize > 0)
        parts << QString("%1 KB").arg(qMax<qint64>(1, asset.fileSize / 1024));
    return parts.join("  |  ");
}

QString compactFileName(const QString &fileName)
{
    QFileInfo info(fileName);
    QString base = info.completeBaseName();
    return base.isEmpty() ? fileName : base;
}

QString sourceBadge(const Asset &asset)
{
    const QString source = asset.metadataSource;
    if (source == "stable-diffusion") return QStringLiteral("SD");
    if (source == "midjourney") return QStringLiteral("Midjourney");
    if (source == "dalle") return QStringLiteral("DALL-E");
    return source.left(8).toUpper();
}

QColor sourceBadgeColor(const Asset &asset)
{
    const QString source = asset.metadataSource;
    if (source == "stable-diffusion") return QColor(0x3d, 0xa3, 0x5d);
    if (source == "midjourney") return QColor(0xa9, 0x70, 0xff);
    if (source == "dalle") return QColor(0x2f, 0x9d, 0xb7);
    return Color::ACCENT;
}
}

GalleryWidget::GalleryWidget(IImageCache *cache, QWidget *parent)
    : QWidget(parent), m_cache(cache)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(m_itemWidth() + kPadding * 2);

    m_labelFont.setPixelSize(Visual::FontBody);
    m_labelFm = QFontMetrics(m_labelFont);
    m_metaFont.setPixelSize(Visual::FontCaption);
    m_metaFm = QFontMetrics(m_metaFont);
    m_badgeFont = m_metaFont;
    m_badgeFont.setBold(true);
    m_emptyTitleFont = m_labelFont;
    m_emptyTitleFont.setPixelSize(Visual::FontHeading);
    m_emptyTitleFont.setBold(true);
    m_emptyBodyFont = m_labelFont;
    m_controlFont.setPixelSize(Visual::FontControl);
    m_actionFont = m_metaFont;
}

void GalleryWidget::onThumbnailReady(const QString &filePath, const QSize &, const QPixmap &pixmap)
{
    if (!pixmap.isNull())
        m_requestedThumbnails.remove(filePath);
    update();
}

void GalleryWidget::setThumbnailSize(int size)
{
    m_thumbSize = qBound(80, size, 320);
    m_requestedThumbnails.clear();
    layoutItems();
    ensureThumbnailsForVisibleItems();
    update();
}

QVector<Asset> GalleryWidget::selectedAssets() const
{
    QVector<Asset> result;
    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < m_assets.size())
            result.append(m_assets[idx]);
    }
    if (result.isEmpty() && !m_selectedAsset.id.isEmpty()) {
        for (const auto &a : m_assets) {
            if (a.id == m_selectedAsset.id) {
                result.append(m_selectedAsset);
                break;
            }
        }
    }
    return result;
}

void GalleryWidget::setAssets(const QVector<Asset> &assets)
{
    m_assets = assets;
    m_totalAssetCount = assets.size();
    m_hoveredIndex = -1;
    m_lastClickedIndex = -1;
    m_selectedAsset = {};
    m_selectedIndices.clear();
    m_requestedThumbnails.clear();
    m_fileExistsCache.clear();
    m_scrollOffset = 0;
    prefetchFileExistence(assets);
    layoutItems();
    ensureThumbnailsForVisibleItems();
    update();
}

void GalleryWidget::setAssets(const QVector<Asset> &assets, int totalCount)
{
    m_assets = assets;
    m_totalAssetCount = totalCount;
    m_hoveredIndex = -1;
    m_lastClickedIndex = -1;
    m_selectedAsset = {};
    m_selectedIndices.clear();
    m_requestedThumbnails.clear();
    m_fileExistsCache.clear();
    m_scrollOffset = 0;
    prefetchFileExistence(assets);
    layoutItems();
    ensureThumbnailsForVisibleItems();
    update();
}

void GalleryWidget::appendPage(const QVector<Asset> &assets, int totalCount)
{
    m_totalAssetCount = totalCount;
    m_assets.append(assets);
    prefetchFileExistence(assets);
    layoutItems();
    ensureThumbnailsForVisibleItems();
    update();
}

void GalleryWidget::appendAssets(const QVector<Asset> &assets)
{
    m_assets.append(assets);
    m_fileExistsCache.clear();
    layoutItems();
    ensureThumbnailsForVisibleItems();
    update();
}

void GalleryWidget::clearAssets()
{
    m_assets.clear();
    m_hoveredIndex = -1;
    m_lastClickedIndex = -1;
    m_selectedAsset = {};
    m_selectedIndices.clear();
    m_requestedThumbnails.clear();
    m_fileExistsCache.clear();
    m_scrollOffset = 0;
    layoutItems();
    update();
}

int GalleryWidget::selectedAssetIndex() const
{
    if (!m_selectedIndices.isEmpty())
        return *m_selectedIndices.begin();
    for (int i = 0; i < m_assets.size(); i++) {
        if (m_assets[i].id == m_selectedAsset.id)
            return i;
    }
    return -1;
}

QVector<Asset> GalleryWidget::allAssets() const { return m_assets; }

void GalleryWidget::prefetchFileExistence(const QVector<Asset> &assets)
{
    for (const auto &a : assets) {
        if (!m_fileExistsCache.contains(a.filePath))
            m_fileExistsCache.insert(a.filePath, QFileInfo::exists(a.filePath));
    }
}

void GalleryWidget::checkLoadMore()
{
    if (m_assets.size() >= m_totalAssetCount) return;
    int lastVisibleRow = (m_scrollOffset + height()) / (m_itemHeight() + kGap);
    int totalRows = (m_assets.size() + m_columns - 1) / m_columns;
    if (lastVisibleRow >= totalRows - 3)
        emit loadMoreRequested(m_assets.size(), kPageSize);
}

void GalleryWidget::layoutItems()
{
    m_columns = qMax(1, (width() - Visual::GalleryTopPadding) / (m_itemWidth() + kGap));
    int rows = (m_assets.size() + m_columns - 1) / m_columns;
    m_totalHeight = rows * (m_itemHeight() + kGap) + Visual::GalleryTopPadding;
}

QRect GalleryWidget::itemRect(int index) const
{
    if (index < 0 || index >= m_assets.size()) return {};
    int row = index / m_columns;
    int col = index % m_columns;
    int x = Visual::GalleryTopPadding + col * (m_itemWidth() + kGap);
    int y = Visual::GalleryTopPadding + row * (m_itemHeight() + kGap) - m_scrollOffset;
    return QRect(x, y, m_itemWidth(), m_itemHeight());
}

int GalleryWidget::indexAt(const QPoint &pos) const
{
    for (int i = 0; i < m_assets.size(); i++) {
        if (itemRect(i).contains(pos))
            return i;
    }
    return -1;
}

QRect GalleryWidget::emptyFolderButtonRect() const
{
    const int totalW = Visual::GalleryEmptyButtonWidth * 2 + Visual::SpaceSm;
    const int x = rect().center().x() - totalW / 2;
    const int y = rect().center().y() + 62;
    return QRect(x, y, Visual::GalleryEmptyButtonWidth, Visual::GalleryEmptyButtonHeight);
}

QRect GalleryWidget::emptyFilesButtonRect() const
{
    return emptyFolderButtonRect().translated(Visual::GalleryEmptyButtonWidth + Visual::SpaceSm, 0);
}

void GalleryWidget::drawEmptyState(QPainter &p)
{
    QRect center = rect().adjusted(40, 0, -40, 0);
    const bool searching = !m_searchKeyword.isEmpty();

    Codicon::draw(p, searching ? "search" : "image",
                  QRect(center.center().x() - 24, center.center().y() - 100, 48, 48),
                  Color::TEXT_SECONDARY, 36);

    p.setFont(m_emptyTitleFont);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(center.adjusted(0, -44, 0, 0), Qt::AlignCenter,
               searching
                   ? tr("没有找到匹配“%1”的素材").arg(m_searchKeyword)
                   : tr("把 AI 素材放进来，开始整理"));

    p.setFont(m_emptyBodyFont);
    p.setPen(Color::TEXT_SECONDARY);
    p.drawText(center.adjusted(0, -4, 0, 0), Qt::AlignCenter,
               searching
                   ? tr("试试放宽来源、收藏或关键词筛选")
                   : tr("导入文件夹可持续整理素材；导入图片适合临时收集"));

    if (searching)
        return;

    const QVector<QPair<QRect, QString>> buttons = {
        { emptyFolderButtonRect(), tr("导入文件夹") },
        { emptyFilesButtonRect(), tr("导入图片") }
    };
    const QVector<QString> icons = { "folder-opened", "file-media" };

    p.setFont(m_controlFont);

    for (int i = 0; i < buttons.size(); i++) {
        QRect r = buttons[i].first;
        bool hovered = (m_emptyHoveredButton == i);
        QPainterPath path;
        path.addRoundedRect(QRectF(r), Visual::RadiusSmall, Visual::RadiusSmall);
        p.fillPath(path, i == 0 ? Color::ACCENT : (hovered ? Color::BG_BUTTON_HOVER : Color::BG_BUTTON));
        p.setPen(QPen(i == 0 ? Color::FOCUS_BORDER : Color::BORDER_SUBTLE, 1));
        p.drawPath(path);

        QColor textColor = i == 0 ? Color::TEXT_BRIGHT : Color::TEXT_PRIMARY;
        Codicon::draw(p, icons[i], QRect(r.left() + 12, r.top(), 16, r.height()), textColor, 13);
        p.setPen(textColor);
        p.drawText(r.adjusted(34, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, buttons[i].second);
    }
}

void GalleryWidget::drawBatchToolbar(QPainter &p)
{
    if (m_selectedIndices.size() <= 1) {
        m_batchTagRect = {};
        m_batchDeleteRect = {};
        m_batchClearRect = {};
        return;
    }

    const int barW = 390;
    const int barH = 36;
    QRect bar(width() - barW - 18, 12, barW, barH);
    QPainterPath barPath;
    barPath.addRoundedRect(QRectF(bar), Visual::RadiusMedium, Visual::RadiusMedium);
    p.fillPath(barPath, Color::BG_MEDIUM);
    p.setPen(QPen(Color::BORDER_SUBTLE, 1));
    p.drawPath(barPath);

    QFont f(p.font());
    f.setPixelSize(Visual::FontControl);
    f.setBold(true);
    p.setFont(f);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(bar.adjusted(14, 0, -260, 0), Qt::AlignVCenter | Qt::AlignLeft,
               tr("已选择 %1 项").arg(m_selectedIndices.size()));

    auto drawAction = [&](QRect &target, int x, int w, const QString &icon, const QString &text, const QColor &color) {
        target = QRect(x, bar.top() + 5, w, 26);
        QPainterPath path;
        path.addRoundedRect(QRectF(target), Visual::RadiusSmall, Visual::RadiusSmall);
        p.fillPath(path, Color::BG_BUTTON_SUBTLE);
        p.setPen(QPen(Color::BORDER_FAINT, 1));
        p.drawPath(path);
        Codicon::draw(p, icon, QRect(target.left() + 8, target.top(), 14, target.height()), color, 12);
        QFont actionFont = p.font();
        actionFont.setPixelSize(Visual::FontCaption);
        actionFont.setBold(false);
        p.setFont(actionFont);
        p.setPen(color);
        p.drawText(target.adjusted(27, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    };

    drawAction(m_batchTagRect, bar.right() - 214, 82, "tag", tr("标签"), Color::TEXT_PRIMARY);
    drawAction(m_batchDeleteRect, bar.right() - 126, 72, "trash", tr("删除"), Color::ERROR_TEXT);
    m_batchClearRect = QRect(bar.right() - 45, bar.top() + 5, 28, 26);
    QPainterPath clearPath;
    clearPath.addRoundedRect(QRectF(m_batchClearRect), Visual::RadiusSmall, Visual::RadiusSmall);
    p.fillPath(clearPath, Color::BG_BUTTON_SUBTLE);
    p.setPen(QPen(Color::BORDER_FAINT, 1));
    p.drawPath(clearPath);
    Codicon::draw(p, "close", m_batchClearRect, Color::TEXT_SECONDARY, 12);
}

void GalleryWidget::clearSelection()
{
    m_isRubberBanding = false;
    m_selectedIndices.clear();
    m_selectedAsset = {};
    m_lastClickedIndex = -1;
    update();
    emit assetSelected({});
}

void GalleryWidget::navigateTo(int index)
{
    if (index < 0 || index >= m_assets.size()) return;
    m_selectedAsset = m_assets[index];
    if (!m_selectedIndices.contains(index)) {
        m_selectedIndices.clear();
        m_selectedIndices.insert(index);
    }
    QRect r = itemRect(index);
    if (r.top() < 0)
        m_scrollOffset += r.top();
    else if (r.bottom() > height())
        m_scrollOffset += r.bottom() - height();
    m_scrollOffset = qBound(0, m_scrollOffset, qMax(0, m_totalHeight - height()));
    ensureThumbnailsForVisibleItems();
    update();
    emit assetSelected(m_selectedAsset);
}

void GalleryWidget::ensureThumbnailsForVisibleItems()
{
    if (m_assets.isEmpty() || m_columns == 0) return;
    int rowH = m_itemHeight() + kGap;
    int firstRow = qMax(0, m_scrollOffset / rowH);
    int visibleRows = height() / rowH + 2;
    int startIdx = qMin(firstRow * m_columns, m_assets.size());
    int endIdx = qMin((firstRow + visibleRows) * m_columns, m_assets.size());
    for (int i = startIdx; i < endIdx; i++) {
        const Asset &asset = m_assets[i];
        QSize thumbSize(m_thumbSize, m_thumbSize);
        if (!m_requestedThumbnails.contains(asset.filePath)) {
            QPixmap thumb = m_cache->get(asset.filePath, thumbSize);
            if (thumb.isNull()) {
                m_requestedThumbnails.insert(asset.filePath);
                m_cache->requestThumbnail(asset.filePath, thumbSize);
            }
        }
    }
}

void GalleryWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), Color::BG_DARKEST);

    if (m_isDragOver) {
        QPen dashPen(Color::ACCENT, 2, Qt::DashLine);
        p.setPen(dashPen);
        p.setBrush(QColor(Color::ACCENT.red(), Color::ACCENT.green(), Color::ACCENT.blue(), 20));
        p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 8, 8);
        QFont df = p.font();
        df.setPixelSize(Visual::FontHeading);
        p.setFont(df);
        p.setPen(Color::ACCENT);
        p.drawText(rect(), Qt::AlignCenter, tr("拖放文件以导入"));
        return;
    }

    if (m_assets.isEmpty() || m_columns == 0) {
        drawEmptyState(p);
        return;
    }

    int rowH = m_itemHeight() + kGap;
    int firstRow = qMax(0, m_scrollOffset / rowH);
    int visibleRows = height() / rowH + 2;
    int startIdx = qMin(firstRow * m_columns, m_assets.size());
    int endIdx = qMin((firstRow + visibleRows) * m_columns, m_assets.size());

    for (int i = startIdx; i < endIdx; i++) {
        const Asset &asset = m_assets[i];
        QRect r = itemRect(i);
        bool isHovered = (i == m_hoveredIndex);
        bool isSelected = m_selectedIndices.contains(i);

        if (isSelected || isHovered) {
            QRect shadowRect = isHovered ? r.adjusted(0, 4, 0, 10) : r.adjusted(2, 2, 2, 6);
            p.setPen(Qt::NoPen);
            p.setBrush(isHovered ? QColor(0, 0, 0, 80) : Color::OVERLAY_SHADOW);
            p.drawRoundedRect(shadowRect, Visual::RadiusMedium, Visual::RadiusMedium);
        }

        QPainterPath cardPath;
        cardPath.addRoundedRect(QRectF(r), Visual::RadiusMedium, Visual::RadiusMedium);
        p.fillPath(cardPath, isSelected ? Color::BG_CARD_HOVER
                                        : (isHovered ? Color::BG_CARD_HOVER : Color::BG_CARD));
        p.setPen(QPen(isSelected ? Color::FOCUS_BORDER
                                 : (isHovered ? Color::BORDER_SUBTLE : Color::BORDER_FAINT),
                      1));
        p.drawPath(cardPath);

        if (isSelected) {
            p.fillRect(QRect(r.left(), r.top() + 8, 3, r.height() - 16), Color::FOCUS_BORDER);
            p.setPen(QPen(Color::WHITE_15, 1));
            p.drawLine(r.left() + 4, r.top() + 2, r.right() - 4, r.top() + 2);
        }

        QRect thumbArea(r.left() + kPadding, r.top() + kPadding, m_thumbSize, m_thumbSize);
        p.setBrush(Color::BG_PREVIEW);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(thumbArea, Visual::RadiusSmall, Visual::RadiusSmall);

        QSize thumbSize(m_thumbSize, m_thumbSize);
        bool fileExists = m_fileExistsCache.value(asset.filePath, true);
        if (!fileExists) {
            p.setPen(Color::ERROR_TEXT);
            p.drawText(thumbArea, Qt::AlignCenter, tr("文件不存在"));
        } else if (m_requestedThumbnails.contains(asset.filePath)) {
            p.setPen(Color::TEXT_SECONDARY);
            p.drawText(thumbArea, Qt::AlignCenter, asset.format.isEmpty() ? tr("加载中") : asset.format.toUpper());
        } else {
            QPixmap thumb = m_cache->get(asset.filePath, thumbSize);
            if (thumb.isNull()) {
                p.setPen(Color::TEXT_SECONDARY);
                p.drawText(thumbArea, Qt::AlignCenter, asset.format.isEmpty() ? tr("加载中") : asset.format.toUpper());
            } else {
                QPainterPath clipPath;
                clipPath.addRoundedRect(QRectF(thumbArea), Visual::RadiusSmall, Visual::RadiusSmall);
                p.save();
                p.setClipPath(clipPath);
                p.drawPixmap(thumbArea.topLeft(), thumb);
                p.restore();
            }
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Color::BORDER_MUTED, 1));
        p.drawRoundedRect(thumbArea, Visual::RadiusSmall, Visual::RadiusSmall);

        const bool hasSource = !asset.metadataSource.isEmpty();
        const bool hasPrompt = !asset.metadataPrompt.trimmed().isEmpty();
        if (hasSource) {
            QString badge = sourceBadge(asset);
            p.setFont(m_badgeFont);
            const int badgeW = qMin(96, p.fontMetrics().horizontalAdvance(badge) + 16);
            QRect badgeRect(thumbArea.left() + 8, thumbArea.top() + 8, badgeW, 20);
            QPainterPath badgePath;
            badgePath.addRoundedRect(QRectF(badgeRect), Visual::RadiusSmall, Visual::RadiusSmall);
            QColor badgeBg = sourceBadgeColor(asset);
            badgeBg.setAlpha(210);
            p.fillPath(badgePath, badgeBg);
            p.setPen(Color::TEXT_BRIGHT);
            p.drawText(badgeRect, Qt::AlignCenter, badge);
        }
        if (hasPrompt) {
            QRect promptRect(thumbArea.right() - 32, thumbArea.top() + 8, 24, 20);
            QPainterPath promptPath;
            promptPath.addRoundedRect(QRectF(promptRect), Visual::RadiusSmall, Visual::RadiusSmall);
            p.fillPath(promptPath, QColor(0, 0, 0, 150));
            Codicon::draw(p, "quote", promptRect, Color::TEXT_PRIMARY, 13);
        }

        QRect labelRect(r.left() + kPadding, r.top() + kPadding + m_thumbSize + 8,
                        r.width() - kPadding * 2 - 32, 16);
        p.setFont(m_labelFont);
        QString fileName = compactFileName(asset.fileName);
        QString elided = m_labelFm.elidedText(fileName, Qt::ElideRight, labelRect.width());
        if (!m_searchKeyword.isEmpty() && fileName.contains(m_searchKeyword, Qt::CaseInsensitive))
            p.fillRect(labelRect.adjusted(0, 2, 0, -2), Color::SEARCH_HIGHLIGHT);
        p.setPen(isSelected ? Color::TEXT_BRIGHT : Color::TEXT_PRIMARY);
        p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, elided);

        QRect metaRect(labelRect.left(), labelRect.bottom() + 1, r.width() - kPadding * 2 - 32, 14);
        p.setFont(m_metaFont);
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter,
                   m_metaFm.elidedText(metaLine(asset), Qt::ElideRight, metaRect.width()));

        QRect starRect(r.right() - 40, r.bottom() - 40, 32, 32);
        if (asset.isFavorite || isHovered || isSelected) {
            QColor starColor = asset.isFavorite ? Color::FAVORITE_ON : Color::FAVORITE_OFF;
            if (isHovered || isSelected) {
                QPainterPath starPath;
                starPath.addRoundedRect(QRectF(starRect.adjusted(4, 4, -4, -4)), Visual::RadiusSmall, Visual::RadiusSmall);
                p.fillPath(starPath, QColor(0, 0, 0, 110));
            }
            Codicon::draw(p, asset.isFavorite ? "star" : "star-empty", starRect, starColor, 15);
        }
    }

    drawBatchToolbar(p);
    drawRubberBand(p);
}

void GalleryWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        auto sel = selectedAssets();
        if (!sel.isEmpty()) emit deleteRequested(sel);
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        m_isRubberBanding = false;
        m_selectedIndices.clear();
        m_selectedAsset = {};
        update();
        emit assetSelected({});
        return;
    }
    if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        m_selectedIndices.clear();
        for (int i = 0; i < m_assets.size(); i++) m_selectedIndices.insert(i);
        if (!m_assets.isEmpty()) {
            m_selectedAsset = m_assets.first();
            emit assetSelected(m_selectedAsset);
        }
        update();
        return;
    }

    int currentIdx = -1;
    if (m_lastClickedIndex >= 0 && m_lastClickedIndex < m_assets.size())
        currentIdx = m_lastClickedIndex;
    else if (!m_selectedIndices.isEmpty())
        currentIdx = *m_selectedIndices.begin();
    else if (!m_selectedAsset.id.isEmpty())
        currentIdx = m_assets.indexOf(m_selectedAsset);

    if (event->key() == Qt::Key_Left && currentIdx > 0) {
        navigateTo(currentIdx - 1);
    } else if (event->key() == Qt::Key_Right && currentIdx >= 0 && currentIdx < m_assets.size() - 1) {
        navigateTo(currentIdx + 1);
    } else if (event->key() == Qt::Key_Up) {
        if (currentIdx >= m_columns)
            navigateTo(currentIdx - m_columns);
        else if (currentIdx < 0 && !m_assets.isEmpty())
            navigateTo(m_assets.size() - 1);
    } else if (event->key() == Qt::Key_Down) {
        int next = currentIdx < 0 ? 0 : currentIdx + m_columns;
        if (next < m_assets.size()) navigateTo(next);
    } else if (event->key() == Qt::Key_Home && !m_assets.isEmpty()) {
        navigateTo(0);
    } else if (event->key() == Qt::Key_End && !m_assets.isEmpty()) {
        navigateTo(m_assets.size() - 1);
    } else if (event->key() == Qt::Key_PageUp && currentIdx >= m_columns) {
        int rowsPerPage = qMax(1, height() / (m_itemHeight() + kGap));
        navigateTo(qMax(0, currentIdx - rowsPerPage * m_columns));
    } else if (event->key() == Qt::Key_PageDown && currentIdx >= 0) {
        int rowsPerPage = qMax(1, height() / (m_itemHeight() + kGap));
        navigateTo(qMin(m_assets.size() - 1, currentIdx + rowsPerPage * m_columns));
    } else if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) {
        if (!m_selectedAsset.id.isEmpty()) emit assetDoubleClicked(m_selectedAsset);
    }

    QWidget::keyPressEvent(event);
}

void GalleryWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isRubberBanding && (event->buttons() & Qt::LeftButton)) {
        QRect oldBand = QRect(m_rubberBandStart, m_rubberBandEnd).normalized();
        m_rubberBandEnd = event->pos();
        updateRubberBandSelection(oldBand);
        return;
    }

    int oldHover = m_hoveredIndex;
    m_hoveredIndex = indexAt(event->pos());
    if (oldHover != m_hoveredIndex) {
        if (oldHover >= 0) update(itemRect(oldHover));
        if (m_hoveredIndex >= 0) update(itemRect(m_hoveredIndex));
    }
    int oldEmpty = m_emptyHoveredButton;
    m_emptyHoveredButton = -1;
    if (m_assets.isEmpty() && m_searchKeyword.isEmpty()) {
        if (emptyFolderButtonRect().contains(event->pos())) m_emptyHoveredButton = 0;
        else if (emptyFilesButtonRect().contains(event->pos())) m_emptyHoveredButton = 1;
    }
    if (oldEmpty != m_emptyHoveredButton)
        update();
}

void GalleryWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (m_selectedIndices.size() > 1) {
        if (m_batchTagRect.contains(event->pos())) {
            QVector<QString> ids;
            for (int idx : m_selectedIndices) {
                if (idx >= 0 && idx < m_assets.size())
                    ids.append(m_assets[idx].id);
            }
            if (!ids.isEmpty()) emit tagAddRequested(ids);
            return;
        }
        if (m_batchDeleteRect.contains(event->pos())) {
            auto sel = selectedAssets();
            if (!sel.isEmpty()) emit deleteRequested(sel);
            return;
        }
        if (m_batchClearRect.contains(event->pos())) {
            clearSelection();
            return;
        }
    }

    if (m_assets.isEmpty() && m_searchKeyword.isEmpty()) {
        if (emptyFolderButtonRect().contains(event->pos())) {
            emit importFolderRequested();
            return;
        }
        if (emptyFilesButtonRect().contains(event->pos())) {
            emit importFilesRequested();
            return;
        }
    }

    int idx = indexAt(event->pos());

    if (idx >= 0) {
        QRect r = itemRect(idx);
        QRect starRect(r.right() - 40, r.bottom() - 40, 32, 32);
        if (starRect.contains(event->pos())) {
            QString assetId = m_assets[idx].id;
            bool newFav = !m_assets[idx].isFavorite;
            m_assets[idx].isFavorite = newFav;
            update();
            emit favoriteToggled(assetId, newFav);
            return;
        }
    }

    if (event->modifiers() & Qt::ControlModifier) {
        if (idx >= 0) {
            m_lastClickedIndex = idx;
            if (m_selectedIndices.contains(idx)) {
                m_selectedIndices.remove(idx);
                if (m_selectedIndices.isEmpty()) {
                    m_selectedAsset = {};
                    emit assetSelected({});
                } else {
                    int firstIdx = *m_selectedIndices.constBegin();
                    m_selectedAsset = m_assets[firstIdx];
                    emit assetSelected(m_selectedAsset);
                }
            } else {
                m_selectedIndices.insert(idx);
                m_selectedAsset = m_assets[idx];
                emit assetSelected(m_selectedAsset);
            }
            update();
        }
    } else if (event->modifiers() & Qt::ShiftModifier) {
        int lastIdx = m_lastClickedIndex;
        if (lastIdx >= 0 && idx >= 0) {
            int start = qMin(lastIdx, idx);
            int end = qMax(lastIdx, idx);
            m_selectedIndices.clear();
            for (int i = start; i <= end; i++) m_selectedIndices.insert(i);
            m_selectedAsset = m_assets[idx];
            update();
            emit assetSelected(m_selectedAsset);
        } else if (idx >= 0) {
            m_selectedIndices.clear();
            m_selectedIndices.insert(idx);
            m_selectedAsset = m_assets[idx];
            update();
            emit assetSelected(m_selectedAsset);
        }
    } else if (idx < 0) {
        m_isRubberBanding = true;
        m_rubberBandStart = event->pos();
        m_rubberBandEnd = event->pos();
        m_rubberBandOrigSelection = m_selectedIndices;
        m_rubberBandAdditive = event->modifiers() & Qt::ShiftModifier;
        m_rubberBandToggle = event->modifiers() & Qt::ControlModifier;
        m_rubberBandSubtract = event->modifiers() & Qt::AltModifier;
        if (!m_rubberBandAdditive && !m_rubberBandToggle && !m_rubberBandSubtract) {
            m_selectedIndices.clear();
            m_selectedAsset = {};
        }
    } else {
        m_selectedIndices.clear();
        m_lastClickedIndex = idx;
        m_selectedAsset = m_assets[idx];
        m_selectedIndices.insert(idx);
        update();
        emit assetSelected(m_assets[idx]);
    }
}

void GalleryWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isRubberBanding) {
        m_isRubberBanding = false;
        QPoint delta = event->pos() - m_rubberBandStart;
        if (qAbs(delta.x()) < 3 && qAbs(delta.y()) < 3) {
            clearSelection();
        } else {
            if (!m_selectedIndices.isEmpty()) {
                m_lastClickedIndex = *m_selectedIndices.constBegin();
            }
            emit assetSelected({});
        }
        update();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void GalleryWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    int idx = indexAt(event->pos());
    if (idx >= 0) emit assetDoubleClicked(m_assets[idx]);
}

void GalleryWidget::drawRubberBand(QPainter &p)
{
    if (!m_isRubberBanding) return;
    QRect band = QRect(m_rubberBandStart, m_rubberBandEnd).normalized();
    if (band.isEmpty()) return;

    QColor fill = Color::ACCENT;
    fill.setAlpha(25);
    p.fillRect(band, fill);

    QPen pen(Color::ACCENT, 1, Qt::DashLine);
    p.setPen(pen);
    p.drawRect(band);
}

void GalleryWidget::updateRubberBandSelection(const QRect &oldBand)
{
    QRect band = QRect(m_rubberBandStart, m_rubberBandEnd).normalized();
    if (band.isEmpty() && oldBand.isEmpty()) return;

    int rowH = m_itemHeight() + kGap;
    int bandTop = band.top() + m_scrollOffset;
    int bandBottom = band.bottom() + m_scrollOffset;
    int firstRow = qMax(0, bandTop / rowH - 1);
    int lastRow = qMin((m_assets.size() - 1) / m_columns, bandBottom / rowH + 1);
    int startIdx = firstRow * m_columns;
    int endIdx = qMin(m_assets.size(), (lastRow + 1) * m_columns);

    QVarLengthArray<int, 32> intersected;
    for (int i = startIdx; i < endIdx; i++) {
        if (itemRect(i).intersects(band))
            intersected.append(i);
    }

    QSet<int> newSelection;
    if (m_rubberBandSubtract) {
        newSelection = m_rubberBandOrigSelection;
        for (int i : intersected) newSelection.remove(i);
    } else if (m_rubberBandToggle) {
        newSelection = m_rubberBandOrigSelection;
        for (int i : intersected) {
            if (newSelection.contains(i)) newSelection.remove(i);
            else newSelection.insert(i);
        }
    } else if (m_rubberBandAdditive) {
        newSelection = m_rubberBandOrigSelection;
        for (int i : intersected) newSelection.insert(i);
    } else {
        newSelection.clear();
        for (int i : intersected) newSelection.insert(i);
    }

    if (newSelection != m_selectedIndices) {
        m_selectedIndices = newSelection;
        update();
    } else {
        QRect dirty = oldBand.united(band).adjusted(-2, -2, 2, 2);
        update(dirty);
    }
}

void GalleryWidget::wheelEvent(QWheelEvent *event)
{
    int delta;
    if (!event->pixelDelta().isNull())
        delta = event->pixelDelta().y();
    else
        delta = event->angleDelta().y() / 8;
    m_scrollOffset -= delta;
    m_scrollOffset = qBound(0, m_scrollOffset, qMax(0, m_totalHeight - height()));
    int rowH = m_itemHeight() + kGap;
    if (rowH > 0) {
        int snapRow = (m_scrollOffset + rowH / 2) / rowH;
        m_scrollOffset = qBound(0, snapRow * rowH, qMax(0, m_totalHeight - height()));
    }
    ensureThumbnailsForVisibleItems();
    update();
    checkLoadMore();
}

void GalleryWidget::resizeEvent(QResizeEvent *)
{
    layoutItems();
    m_scrollOffset = qBound(0, m_scrollOffset, qMax(0, m_totalHeight - height()));
    ensureThumbnailsForVisibleItems();
    update();
}

void GalleryWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int idx = indexAt(event->pos());
    if (idx < 0 && m_selectedIndices.isEmpty() && m_selectedAsset.id.isEmpty()) return;

    QMenu menu(this);
    UIUtils::setupMenuPalette(menu);

    auto sel = selectedAssets();
    if (sel.size() == 1) {
        bool isFav = sel[0].isFavorite;
        QAction *favAction = menu.addAction(isFav ? tr("取消收藏") : tr("收藏"));
        favAction->setIcon(Codicon::cachedIcon(isFav ? "star-empty" : "star",
                                               isFav ? Color::FAVORITE_OFF : Color::FAVORITE_ON, 14));
        connect(favAction, &QAction::triggered, this, [this, sel]() {
            emit favoriteToggled(sel[0].id, !sel[0].isFavorite);
        });
    }
    if (!sel.isEmpty()) {
        menu.addSeparator();
        QAction *tagAction = menu.addAction(tr("添加标签..."));
        tagAction->setIcon(Codicon::cachedIcon("tag", Color::TEXT_PRIMARY, 14));
        connect(tagAction, &QAction::triggered, this, [this, sel]() {
            QVector<QString> ids;
            for (const auto &a : sel) ids.append(a.id);
            emit tagAddRequested(ids);
        });
        menu.addSeparator();
        QAction *delAction = menu.addAction(tr("删除"));
        delAction->setIcon(Codicon::cachedIcon("trash", Color::TEXT_PRIMARY, 14));
        delAction->setShortcut(QKeySequence::Delete);
        connect(delAction, &QAction::triggered, this, [this, sel]() {
            emit deleteRequested(sel);
        });
    }
    menu.exec(event->globalPos());
}

void GalleryWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        m_isDragOver = true;
        update();
        event->acceptProposedAction();
    }
}

void GalleryWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        m_isDragOver = true;
        event->acceptProposedAction();
    }
}

void GalleryWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_isDragOver = false;
    update();
    QWidget::dragLeaveEvent(event);
}

void GalleryWidget::dropEvent(QDropEvent *event)
{
    m_isDragOver = false;
    QStringList paths;
    for (const auto &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    if (!paths.isEmpty()) {
        emit filesDropped(paths);
        event->acceptProposedAction();
    }
}

bool GalleryWidget::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(event);
        int idx = indexAt(he->pos());
        if (idx >= 0 && idx < m_assets.size()) {
            const Asset &a = m_assets[idx];
            QString tip = QString("%1\n%2").arg(a.fileName).arg(metaLine(a));
            QToolTip::showText(he->globalPos(), tip, this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}
