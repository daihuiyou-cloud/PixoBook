#include "GalleryWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMimeData>
#include <QUrl>
#include <QFontMetrics>
#include <QAction>
#include <QFileInfo>
#include "ui/Codicon.h"

GalleryWidget::GalleryWidget(IImageCache *cache, QWidget *parent)
    : QWidget(parent), m_cache(cache)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(m_itemWidth() + kPadding * 2);
}

void GalleryWidget::onThumbnailReady(const QString &, const QSize &, const QPixmap &)
{
    update();
}

void GalleryWidget::setThumbnailSize(int size)
{
    m_thumbSize = qBound(80, size, 320);
    m_columns = qMax(1, width() / m_itemWidth());
    layoutItems();
    update();
}

QVector<Asset> GalleryWidget::selectedAssets() const
{
    QVector<Asset> result;
    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < m_assets.size())
            result.append(m_assets[idx]);
    }
    if (result.isEmpty() && !m_selectedAsset.id.isEmpty())
        result.append(m_selectedAsset);
    return result;
}

void GalleryWidget::setAssets(const QVector<Asset> &assets)
{
    m_assets = assets;
    m_hoveredIndex = -1;
    m_selectedAsset = {};
    m_selectedIndices.clear();
    m_scrollOffset = 0;
    layoutItems();
    update();
}

void GalleryWidget::appendAssets(const QVector<Asset> &assets)
{
    m_assets.append(assets);
    layoutItems();
    update();
}

void GalleryWidget::clearAssets()
{
    m_assets.clear();
    m_hoveredIndex = -1;
    m_selectedAsset = {};
    m_selectedIndices.clear();
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

QVector<Asset> GalleryWidget::allAssets() const
{
    return m_assets;
}

void GalleryWidget::layoutItems()
{
    m_columns = qMax(1, width() / m_itemWidth());
    if (m_columns == 0) m_columns = 1;
    int rows = (m_assets.size() + m_columns - 1) / m_columns;
    m_totalHeight = rows * m_itemHeight() + kPadding;
}

QRect GalleryWidget::itemRect(int index) const
{
    if (index < 0 || index >= m_assets.size()) return {};
    int row = index / m_columns;
    int col = index % m_columns;
    int x = kPadding + col * m_itemWidth();
    int y = kPadding + row * m_itemHeight() - m_scrollOffset;
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

void GalleryWidget::navigateTo(int index)
{
    if (index < 0 || index >= m_assets.size()) return;
    m_selectedIndices.clear();
    m_selectedAsset = m_assets[index];
    m_selectedIndices.insert(index);
    // Scroll to make visible
    QRect r = itemRect(index);
    if (r.top() < 0) {
        m_scrollOffset += r.top();
    } else if (r.bottom() > height()) {
        m_scrollOffset += r.bottom() - height();
    }
    m_scrollOffset = qBound(0, m_scrollOffset, qMax(0, m_totalHeight - height()));
    update();
    emit assetSelected(m_selectedAsset);
}

void GalleryWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    p.fillRect(rect(), QColor(0x1e, 0x1e, 0x1e));

    if (m_assets.isEmpty() || m_columns == 0) {
        p.setPen(QColor(0x96, 0x96, 0x96));
        QFont emptyFont = p.font();
        emptyFont.setPointSize(14);
        p.setFont(emptyFont);
        if (!m_searchKeyword.isEmpty()) {
            QString msg = QStringLiteral("没有符合 \"%1\" 的素材").arg(m_searchKeyword);
            p.drawText(rect(), Qt::AlignCenter, msg);
        } else {
            p.drawText(rect(), Qt::AlignCenter,
                       QStringLiteral("将素材文件夹拖拽到这里，或通过菜单导入"));
        }
        return;
    }

    int itemH = m_itemHeight();
    int firstRow = qMax(0, m_scrollOffset / itemH);
    int visibleRows = height() / itemH + 2;
    int startIdx = qMin(firstRow * m_columns, m_assets.size());
    int endIdx = qMin((firstRow + visibleRows) * m_columns, m_assets.size());

    for (int i = startIdx; i < endIdx; i++) {
        QRect r = itemRect(i);

        bool isHovered = (i == m_hoveredIndex);
        bool isSelected = m_selectedIndices.contains(i) || m_selectedAsset.id == m_assets[i].id;

        // Card background
        QPainterPath cardPath;
        cardPath.addRoundedRect(QRectF(r), 6, 6);

        if (isSelected) {
            p.fillPath(cardPath, QColor(0x37, 0x37, 0x3d));
            QRectF leftBorder(r.x() + 1, r.y() + 4, 3, r.height() - 8);
            p.fillRect(leftBorder, QColor(0x00, 0x7a, 0xcc));
        } else if (isHovered) {
            p.fillPath(cardPath, QColor(0x2a, 0x2d, 0x2e));
        } else {
            p.fillPath(cardPath, QColor(0x2d, 0x2d, 0x30));
        }

        p.setPen(QPen(isSelected ? QColor(0x00, 0x7a, 0xcc) : QColor(0x3c, 0x3c, 0x3c), 1));
        p.drawPath(cardPath);

        // Thumbnail area
        QRect thumbArea = r.adjusted(kPadding, kPadding, -kPadding, -kPadding - 26);
        QSize thumbSize(m_thumbSize, m_thumbSize);

        bool fileExists = QFileInfo::exists(m_assets[i].filePath);
        if (!fileExists) {
            p.fillRect(thumbArea, QColor(0x3c, 0x20, 0x20));
            p.setPen(QColor(0xc0, 0x60, 0x60));
            p.drawText(thumbArea, Qt::AlignCenter,
                       QString::fromUtf8("\xe6\x96\x87\xe4\xbb\xb6\xe4\xb8\x8d\xe5\xad\x98\xe5\x9c\xa8"));
        } else {
            QPixmap thumb = m_cache->get(m_assets[i].filePath, thumbSize);
            if (thumb.isNull()) {
                m_cache->requestThumbnail(m_assets[i].filePath, thumbSize);
                p.fillRect(thumbArea, QColor(0x3c, 0x3c, 0x3f));
                p.setPen(QColor(0x96, 0x96, 0x96));
                p.drawText(thumbArea, Qt::AlignCenter, m_assets[i].format.toUpper());
            } else {
                p.setPen(QPen(QColor(0x3c, 0x3c, 0x3c), 1));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(thumbArea, 4, 4);

                QPainterPath clipPath;
                clipPath.addRoundedRect(QRectF(thumbArea), 4, 4);
                p.setClipPath(clipPath);
                p.drawPixmap(thumbArea.topLeft(), thumb);
                p.setClipping(false);
            }
        }

        // File name label
        QRect labelRect = r.adjusted(kPadding, kPadding + m_thumbSize + 6, -kPadding - 20, 0);
        QString fileName = m_assets[i].fileName;
        QString elided = p.fontMetrics().elidedText(fileName, Qt::ElideRight, labelRect.width());

        if (!m_searchKeyword.isEmpty() && elided.contains(m_searchKeyword, Qt::CaseInsensitive)) {
            QRect bgRect = labelRect;
            bgRect.setHeight(p.fontMetrics().height());
            p.fillRect(bgRect, QColor(0x2a, 0x2a, 0x2d));
            p.setPen(QColor(0xcc, 0xcc, 0xcc));
            p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
        } else {
            p.setPen(QColor(0xcc, 0xcc, 0xcc));
            p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
        }

        // Favorite star
        QRect starRect(r.right() - 26, r.bottom() - 24, 22, 22);
        QColor starColor = m_assets[i].isFavorite ? QColor(0xff, 0xcc, 0x00) : QColor(0x60, 0x60, 0x60);
        Codicon::draw(p, "star", starRect, starColor, 14);
    }
}

void GalleryWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        auto sel = selectedAssets();
        if (!sel.isEmpty())
            emit deleteRequested(sel);
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        m_selectedIndices.clear();
        m_selectedAsset = {};
        update();
        emit assetSelected({});
        return;
    }
    if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        m_selectedIndices.clear();
        for (int i = 0; i < m_assets.size(); i++)
            m_selectedIndices.insert(i);
        if (!m_assets.isEmpty()) {
            m_selectedAsset = m_assets.first();
            emit assetSelected(m_selectedAsset);
        }
        update();
        return;
    }
    // Arrow navigation
    int currentIdx = -1;
    if (!m_selectedIndices.isEmpty())
        currentIdx = *m_selectedIndices.begin();
    else if (!m_selectedAsset.id.isEmpty())
        currentIdx = m_assets.indexOf(m_selectedAsset);

    if (event->key() == Qt::Key_Left && currentIdx > 0) {
        navigateTo(currentIdx - 1);
    } else if (event->key() == Qt::Key_Right && currentIdx >= 0 && currentIdx < m_assets.size() - 1) {
        navigateTo(currentIdx + 1);
    } else if (event->key() == Qt::Key_Up && currentIdx >= m_columns) {
        navigateTo(currentIdx - m_columns);
    } else if (event->key() == Qt::Key_Down) {
        int next = currentIdx < 0 ? 0 : currentIdx + m_columns;
        if (next < m_assets.size())
            navigateTo(next);
    } else if (event->key() == Qt::Key_Home && !m_assets.isEmpty()) {
        navigateTo(0);
    } else if (event->key() == Qt::Key_End && !m_assets.isEmpty()) {
        navigateTo(m_assets.size() - 1);
    } else if (event->key() == Qt::Key_PageUp && currentIdx >= m_columns) {
        int rowsPerPage = qMax(1, height() / m_itemHeight());
        navigateTo(qMax(0, currentIdx - rowsPerPage * m_columns));
    } else if (event->key() == Qt::Key_PageDown && currentIdx >= 0) {
        int rowsPerPage = qMax(1, height() / m_itemHeight());
        navigateTo(qMin(m_assets.size() - 1, currentIdx + rowsPerPage * m_columns));
    } else if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) {
        if (!m_selectedAsset.id.isEmpty())
            emit assetDoubleClicked(m_selectedAsset);
    }

    QWidget::keyPressEvent(event);
}

void GalleryWidget::mouseMoveEvent(QMouseEvent *event)
{
    int oldHover = m_hoveredIndex;
    m_hoveredIndex = indexAt(event->pos());
    if (oldHover != m_hoveredIndex)
        update();
}

void GalleryWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    int idx = indexAt(event->pos());

    // Favorite star toggle on card
    if (idx >= 0) {
        QRect r = itemRect(idx);
        QRect starRect(r.right() - 28, r.bottom() - 26, 26, 26);
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
        // Toggle multi-select
        if (idx >= 0) {
            if (m_selectedIndices.contains(idx))
                m_selectedIndices.remove(idx);
            else
                m_selectedIndices.insert(idx);
            m_selectedAsset = m_assets[idx];
            update();
            emit assetSelected(m_selectedAsset);
        }
    } else if (event->modifiers() & Qt::ShiftModifier) {
        // Range select from last selection
        int lastIdx = -1;
        if (!m_selectedIndices.isEmpty())
            lastIdx = *m_selectedIndices.begin();
        else if (!m_selectedAsset.id.isEmpty())
            lastIdx = m_assets.indexOf(m_selectedAsset);
        if (lastIdx >= 0 && idx >= 0) {
            int start = qMin(lastIdx, idx);
            int end = qMax(lastIdx, idx);
            m_selectedIndices.clear();
            for (int i = start; i <= end; i++)
                m_selectedIndices.insert(i);
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
    } else {
        m_selectedIndices.clear();
        if (idx >= 0) {
            m_selectedAsset = m_assets[idx];
            m_selectedIndices.insert(idx);
            update();
            emit assetSelected(m_assets[idx]);
        } else {
            m_selectedAsset = {};
            update();
            emit assetSelected({});
        }
    }
}

void GalleryWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    int idx = indexAt(event->pos());
    if (idx >= 0)
        emit assetDoubleClicked(m_assets[idx]);
}

void GalleryWidget::wheelEvent(QWheelEvent *event)
{
    m_scrollOffset -= event->angleDelta().y() / 8;
    m_scrollOffset = qBound(0, m_scrollOffset, qMax(0, m_totalHeight - height()));
    // Snap to nearest row for clean alignment
    int itemH = m_itemHeight();
    if (itemH > 0) {
        int snapRow = (m_scrollOffset + itemH / 2) / itemH;
        m_scrollOffset = qBound(0, snapRow * itemH, qMax(0, m_totalHeight - height()));
    }
    update();
}

void GalleryWidget::resizeEvent(QResizeEvent *)
{
    layoutItems();
    update();
}

void GalleryWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int idx = indexAt(event->pos());

    if (idx < 0 && m_selectedIndices.isEmpty() && m_selectedAsset.id.isEmpty())
        return;

    QMenu menu(this);
    QPalette mPal;
    mPal.setColor(QPalette::Window, QColor(0x25, 0x25, 0x26));
    mPal.setColor(QPalette::WindowText, QColor(0xcc, 0xcc, 0xcc));
    mPal.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
    mPal.setColor(QPalette::HighlightedText, QColor(0xcc, 0xcc, 0xcc));
    menu.setPalette(mPal);

    auto sel = selectedAssets();
    if (sel.size() == 1) {
        QAction *favAction = menu.addAction(sel[0].isFavorite
            ? QString::fromUtf8("\xe5\x8f\x96\xe6\xb6\x88\xe6\x94\xb6\xe8\x97\x8f")
            : QString::fromUtf8("\xe6\x94\xb6\xe8\x97\x8f"));
        connect(favAction, &QAction::triggered, this, [this, sel]() {
            emit favoriteToggled(sel[0].id, !sel[0].isFavorite);
        });
    }
    if (!sel.isEmpty()) {
        menu.addSeparator();
        QAction *tagAction = menu.addAction(QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe6\xa0\x87\xe7\xad\xbe..."));
        connect(tagAction, &QAction::triggered, this, [this, sel]() {
            QVector<QString> ids;
            for (const auto &a : sel) ids.append(a.id);
            emit tagAddRequested(ids);
        });
        menu.addSeparator();
        QAction *delAction = menu.addAction(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4"));
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
        event->acceptProposedAction();
    }
}

void GalleryWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void GalleryWidget::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const auto &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            paths.append(url.toLocalFile());
    }
    if (!paths.isEmpty()) {
        emit filesDropped(paths);
        event->acceptProposedAction();
    }
}
