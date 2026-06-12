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

QIcon menuIcon(const QString &name, const QColor &color)
{
    QPixmap px(16, 16);
    px.fill(Qt::transparent);
    QPainter p(&px);
    Codicon::draw(p, name, QRect(0, 0, 16, 16), color, 14);
    return QIcon(px);
}
}

GalleryWidget::GalleryWidget(IImageCache *cache, QWidget *parent)
    : QWidget(parent), m_cache(cache)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(m_itemWidth() + kPadding * 2);
}

void GalleryWidget::onThumbnailReady(const QString &, const QSize &, const QPixmap &) { update(); }

void GalleryWidget::setThumbnailSize(int size)
{
    m_thumbSize = qBound(80, size, 320);
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

QVector<Asset> GalleryWidget::allAssets() const { return m_assets; }

void GalleryWidget::layoutItems()
{
    m_columns = qMax(1, (width() - kPadding) / (m_itemWidth() + kGap));
    int rows = (m_assets.size() + m_columns - 1) / m_columns;
    m_totalHeight = rows * (m_itemHeight() + kGap) + kPadding;
}

QRect GalleryWidget::itemRect(int index) const
{
    if (index < 0 || index >= m_assets.size()) return {};
    int row = index / m_columns;
    int col = index % m_columns;
    int x = kPadding + col * (m_itemWidth() + kGap);
    int y = kPadding + row * (m_itemHeight() + kGap) - m_scrollOffset;
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
    QRect r = itemRect(index);
    if (r.top() < 0)
        m_scrollOffset += r.top();
    else if (r.bottom() > height())
        m_scrollOffset += r.bottom() - height();
    m_scrollOffset = qBound(0, m_scrollOffset, qMax(0, m_totalHeight - height()));
    update();
    emit assetSelected(m_selectedAsset);
}

void GalleryWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), Color::BG_DARKEST);

    if (m_assets.isEmpty() || m_columns == 0) {
        QRect center = rect().adjusted(32, 0, -32, 0);
        Codicon::draw(p, m_searchKeyword.isEmpty() ? "images" : "search",
                      QRect(center.center().x() - 22, center.center().y() - 78, 44, 44),
                      Color::TEXT_SECONDARY, 34);

        QFont title = p.font();
        title.setPixelSize(Visual::FontHeading);
        title.setBold(true);
        p.setFont(title);
        p.setPen(Color::TEXT_PRIMARY);
        if (!m_searchKeyword.isEmpty()) {
            p.drawText(center.adjusted(0, -24, 0, 0), Qt::AlignCenter,
                       QStringLiteral("没有找到匹配 \"%1\" 的素材").arg(m_searchKeyword));
        } else {
            p.drawText(center.adjusted(0, -24, 0, 0), Qt::AlignCenter,
                       QStringLiteral("拖入图片或文件夹，开始整理 AI 素材"));
        }

        QFont body = p.font();
        body.setPixelSize(Visual::FontBody);
        body.setBold(false);
        p.setFont(body);
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(center.adjusted(0, 18, 0, 0), Qt::AlignCenter,
                   m_searchKeyword.isEmpty()
                       ? QStringLiteral("也可以通过 文件 > 导入 添加素材库目录")
                       : QStringLiteral("试试放宽来源、收藏或关键词筛选"));
        return;
    }

    int rowH = m_itemHeight() + kGap;
    int firstRow = qMax(0, m_scrollOffset / rowH);
    int visibleRows = height() / rowH + 2;
    int startIdx = qMin(firstRow * m_columns, m_assets.size());
    int endIdx = qMin((firstRow + visibleRows) * m_columns, m_assets.size());

    QFont labelFont = p.font();
    labelFont.setPixelSize(Visual::FontBody);
    QFont metaFont = p.font();
    metaFont.setPixelSize(Visual::FontCaption);

    for (int i = startIdx; i < endIdx; i++) {
        const Asset &asset = m_assets[i];
        QRect r = itemRect(i);
        bool isHovered = (i == m_hoveredIndex);
        bool isSelected = m_selectedIndices.contains(i) || m_selectedAsset.id == asset.id;

        QPainterPath cardPath;
        cardPath.addRoundedRect(QRectF(r), Visual::RadiusMedium, Visual::RadiusMedium);
        p.fillPath(cardPath, isSelected ? Color::BG_CARD_HOVER
                                        : (isHovered ? Color::BG_CARD_HOVER : Color::BG_CARD));
        p.setPen(QPen(isSelected ? Color::ACCENT
                                 : (isHovered ? Color::BORDER_SUBTLE : Color::BORDER_MUTED),
                      isSelected ? 2 : 1));
        p.drawPath(cardPath);

        if (isSelected)
            p.fillRect(QRect(r.left(), r.top() + 8, 3, r.height() - 16), Color::ACCENT);

        QRect thumbArea(r.left() + kPadding, r.top() + kPadding, m_thumbSize, m_thumbSize);
        QPainterPath thumbPath;
        thumbPath.addRoundedRect(QRectF(thumbArea), Visual::RadiusSmall, Visual::RadiusSmall);
        p.fillPath(thumbPath, Color::BG_PREVIEW);

        QSize thumbSize(m_thumbSize, m_thumbSize);
        bool fileExists = QFileInfo::exists(asset.filePath);
        if (!fileExists) {
            p.setPen(Color::ERROR_TEXT);
            p.drawText(thumbArea, Qt::AlignCenter, QStringLiteral("文件不存在"));
        } else {
            QPixmap thumb = m_cache->get(asset.filePath, thumbSize);
            if (thumb.isNull()) {
                m_cache->requestThumbnail(asset.filePath, thumbSize);
                p.setPen(Color::TEXT_SECONDARY);
                p.drawText(thumbArea, Qt::AlignCenter,
                           asset.format.isEmpty() ? QStringLiteral("加载中") : asset.format.toUpper());
            } else {
                p.save();
                p.setClipPath(thumbPath);
                p.drawPixmap(thumbArea.topLeft(), thumb);
                p.restore();
            }
        }
        p.setPen(QPen(Color::BORDER_MUTED, 1));
        p.drawPath(thumbPath);

        QRect labelRect(r.left() + kPadding, r.top() + kPadding + m_thumbSize + 8,
                        r.width() - kPadding * 2 - 28, 18);
        p.setFont(labelFont);
        QString fileName = asset.fileName;
        QString elided = p.fontMetrics().elidedText(fileName, Qt::ElideRight, labelRect.width());
        if (!m_searchKeyword.isEmpty() && fileName.contains(m_searchKeyword, Qt::CaseInsensitive)) {
            p.fillRect(labelRect.adjusted(0, 2, 0, -2), Color::SEARCH_HIGHLIGHT);
        }
        p.setPen(isSelected ? Color::TEXT_BRIGHT : Color::TEXT_PRIMARY);
        p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, elided);

        QRect metaRect(labelRect.left(), labelRect.bottom() + 2, r.width() - kPadding * 2 - 28, 16);
        p.setFont(metaFont);
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(metaLine(asset), Qt::ElideRight, metaRect.width()));

        QRect starRect(r.right() - 34, r.bottom() - 34, 26, 26);
        if (asset.isFavorite || isHovered || isSelected) {
            QColor starColor = asset.isFavorite ? Color::FAVORITE_ON : Color::FAVORITE_OFF;
            if (isHovered || isSelected)
                p.fillRect(starRect.adjusted(2, 2, -2, -2), QColor(0, 0, 0, 80));
            Codicon::draw(p, asset.isFavorite ? "star" : "star-empty", starRect, starColor, 15);
        }
    }
}

void GalleryWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        auto sel = selectedAssets();
        if (!sel.isEmpty()) emit deleteRequested(sel);
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
        for (int i = 0; i < m_assets.size(); i++) m_selectedIndices.insert(i);
        if (!m_assets.isEmpty()) {
            m_selectedAsset = m_assets.first();
            emit assetSelected(m_selectedAsset);
        }
        update();
        return;
    }

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
    int oldHover = m_hoveredIndex;
    m_hoveredIndex = indexAt(event->pos());
    if (oldHover != m_hoveredIndex) update();
}

void GalleryWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    int idx = indexAt(event->pos());

    if (idx >= 0) {
        QRect r = itemRect(idx);
        QRect starRect(r.right() - 36, r.bottom() - 36, 32, 32);
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
            if (m_selectedIndices.contains(idx)) m_selectedIndices.remove(idx);
            else m_selectedIndices.insert(idx);
            m_selectedAsset = m_assets[idx];
            update();
            emit assetSelected(m_selectedAsset);
        }
    } else if (event->modifiers() & Qt::ShiftModifier) {
        int lastIdx = -1;
        if (!m_selectedIndices.isEmpty())
            lastIdx = *m_selectedIndices.begin();
        else if (!m_selectedAsset.id.isEmpty())
            lastIdx = m_assets.indexOf(m_selectedAsset);
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
    if (idx >= 0) emit assetDoubleClicked(m_assets[idx]);
}

void GalleryWidget::wheelEvent(QWheelEvent *event)
{
    m_scrollOffset -= event->angleDelta().y() / 8;
    m_scrollOffset = qBound(0, m_scrollOffset, qMax(0, m_totalHeight - height()));
    int rowH = m_itemHeight() + kGap;
    if (rowH > 0) {
        int snapRow = (m_scrollOffset + rowH / 2) / rowH;
        m_scrollOffset = qBound(0, snapRow * rowH, qMax(0, m_totalHeight - height()));
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
    if (idx < 0 && m_selectedIndices.isEmpty() && m_selectedAsset.id.isEmpty()) return;

    QMenu menu(this);
    QPalette mPal;
    mPal.setColor(QPalette::Window, Color::BG_DARK);
    mPal.setColor(QPalette::WindowText, Color::TEXT_PRIMARY);
    mPal.setColor(QPalette::Highlight, Color::HIGHLIGHT);
    mPal.setColor(QPalette::HighlightedText, Color::TEXT_PRIMARY);
    menu.setPalette(mPal);

    auto sel = selectedAssets();
    if (sel.size() == 1) {
        bool isFav = sel[0].isFavorite;
        QAction *favAction = menu.addAction(isFav ? QStringLiteral("取消收藏") : QStringLiteral("收藏"));
        favAction->setIcon(menuIcon(isFav ? "star-empty" : "star",
                                    isFav ? Color::FAVORITE_OFF : Color::FAVORITE_ON));
        connect(favAction, &QAction::triggered, this, [this, sel]() {
            emit favoriteToggled(sel[0].id, !sel[0].isFavorite);
        });
    }
    if (!sel.isEmpty()) {
        menu.addSeparator();
        QAction *tagAction = menu.addAction(QStringLiteral("添加标签..."));
        tagAction->setIcon(menuIcon("tag", Color::TEXT_PRIMARY));
        connect(tagAction, &QAction::triggered, this, [this, sel]() {
            QVector<QString> ids;
            for (const auto &a : sel) ids.append(a.id);
            emit tagAddRequested(ids);
        });
        menu.addSeparator();
        QAction *delAction = menu.addAction(QStringLiteral("删除"));
        delAction->setIcon(menuIcon("trash", Color::TEXT_PRIMARY));
        delAction->setShortcut(QKeySequence::Delete);
        connect(delAction, &QAction::triggered, this, [this, sel]() {
            emit deleteRequested(sel);
        });
    }
    menu.exec(event->globalPos());
}

void GalleryWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void GalleryWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void GalleryWidget::dropEvent(QDropEvent *event)
{
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
            QString tip = QString("%1\n%2")
                              .arg(a.fileName)
                              .arg(metaLine(a));
            QToolTip::showText(he->globalPos(), tip, this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}
