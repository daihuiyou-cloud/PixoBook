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

QString compactFileName(const QString &fileName)
{
    QFileInfo info(fileName);
    QString base = info.completeBaseName();
    return base.isEmpty() ? fileName : base;
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

void GalleryWidget::onThumbnailReady(const QString &filePath, const QSize &, const QPixmap &pixmap)
{
    if (!pixmap.isNull())
        m_requestedThumbnails.remove(filePath);
    update();
}

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
    m_hoveredIndex = -1;
    m_selectedAsset = {};
    m_selectedIndices.clear();
    m_requestedThumbnails.clear();
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
    m_requestedThumbnails.clear();
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

    QFont title = p.font();
    title.setPixelSize(Visual::FontHeading);
    title.setBold(true);
    p.setFont(title);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(center.adjusted(0, -44, 0, 0), Qt::AlignCenter,
               searching
                   ? QStringLiteral("没有找到匹配 \"%1\" 的素材").arg(m_searchKeyword)
                   : QStringLiteral("把 AI 素材放进来，开始整理"));

    QFont body = p.font();
    body.setPixelSize(Visual::FontBody);
    body.setBold(false);
    p.setFont(body);
    p.setPen(Color::TEXT_SECONDARY);
    p.drawText(center.adjusted(0, -4, 0, 0), Qt::AlignCenter,
               searching
                   ? QStringLiteral("试试放宽来源、收藏或关键词筛选")
                   : QStringLiteral("导入文件夹会持续整理素材；导入图片适合临时收集"));

    if (searching)
        return;

    const QVector<QPair<QRect, QString>> buttons = {
        { emptyFolderButtonRect(), QStringLiteral("导入文件夹") },
        { emptyFilesButtonRect(), QStringLiteral("导入图片") }
    };
    const QVector<QString> icons = { "folder-opened", "file-media" };

    QFont buttonFont = p.font();
    buttonFont.setPixelSize(Visual::FontControl);
    p.setFont(buttonFont);

    for (int i = 0; i < buttons.size(); i++) {
        QRect r = buttons[i].first;
        bool hovered = r.contains(mapFromGlobal(QCursor::pos()));
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
        drawEmptyState(p);
        return;
    }

    int rowH = m_itemHeight() + kGap;
    int firstRow = qMax(0, m_scrollOffset / rowH);
    int visibleRows = height() / rowH + 2;
    int startIdx = qMin(firstRow * m_columns, m_assets.size());
    int endIdx = qMin((firstRow + visibleRows) * m_columns, m_assets.size());

    QFont labelFont = p.font();
    labelFont.setPixelSize(Visual::FontBody);
    QFontMetrics labelFm(labelFont);
    QFont metaFont = p.font();
    metaFont.setPixelSize(Visual::FontCaption);
    QFontMetrics metaFm(metaFont);

    for (int i = startIdx; i < endIdx; i++) {
        const Asset &asset = m_assets[i];
        QRect r = itemRect(i);
        bool isHovered = (i == m_hoveredIndex);
        bool isSelected = m_selectedIndices.contains(i);

        if (isSelected || isHovered) {
            QRect shadowRect = r.adjusted(2, 2, 2, 6);
            p.setPen(Qt::NoPen);
            p.setBrush(Color::OVERLAY_SHADOW);
            p.drawRoundedRect(shadowRect, Visual::RadiusMedium, Visual::RadiusMedium);
        }

        p.setBrush(isSelected ? Color::BG_CARD_HOVER
                              : (isHovered ? Color::BG_CARD_HOVER : Color::BG_CARD));
        p.setPen(QPen(isSelected ? Color::ACCENT
                                 : (isHovered ? Color::BORDER_SUBTLE : Color::BORDER_FAINT),
                      isSelected ? 2 : 1));
        p.drawRoundedRect(r, Visual::RadiusMedium, Visual::RadiusMedium);

        if (isSelected) {
            p.fillRect(QRect(r.left(), r.top() + 8, 3, r.height() - 16), Color::ACCENT);
            p.setPen(QPen(Color::WHITE_15, 1));
            p.drawLine(r.left() + 4, r.top() + 2, r.right() - 4, r.top() + 2);
        }

        QRect thumbArea(r.left() + kPadding, r.top() + kPadding, m_thumbSize, m_thumbSize);
        p.setBrush(Color::BG_PREVIEW);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(thumbArea, Visual::RadiusSmall, Visual::RadiusSmall);

        QSize thumbSize(m_thumbSize, m_thumbSize);
        bool fileExists = QFileInfo::exists(asset.filePath);
        if (!fileExists) {
            p.setPen(Color::ERROR_TEXT);
            p.drawText(thumbArea, Qt::AlignCenter, QStringLiteral("文件不存在"));
        } else {
            QPixmap thumb = m_cache->get(asset.filePath, thumbSize);
            if (thumb.isNull()) {
                if (!m_requestedThumbnails.contains(asset.filePath)) {
                    m_requestedThumbnails.insert(asset.filePath);
                    m_cache->requestThumbnail(asset.filePath, thumbSize);
                }
                p.setPen(Color::TEXT_SECONDARY);
                p.drawText(thumbArea, Qt::AlignCenter,
                           asset.format.isEmpty() ? QStringLiteral("加载中") : asset.format.toUpper());
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

        QRect labelRect(r.left() + kPadding, r.top() + kPadding + m_thumbSize + 8,
                        r.width() - kPadding * 2 - 32, 16);
        p.setFont(labelFont);
        QString fileName = compactFileName(asset.fileName);
        QString elided = labelFm.elidedText(fileName, Qt::ElideRight, labelRect.width());
        if (!m_searchKeyword.isEmpty() && fileName.contains(m_searchKeyword, Qt::CaseInsensitive)) {
            p.fillRect(labelRect.adjusted(0, 2, 0, -2), Color::SEARCH_HIGHLIGHT);
        }
        p.setPen(isSelected ? Color::TEXT_BRIGHT : Color::TEXT_PRIMARY);
        p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, elided);

        QRect metaRect(labelRect.left(), labelRect.bottom() + 1, r.width() - kPadding * 2 - 32, 14);
        p.setFont(metaFont);
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter,
                   metaFm.elidedText(metaLine(asset), Qt::ElideRight, metaRect.width()));

        QRect starRect(r.right() - 40, r.bottom() - 40, 32, 32);
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
    int oldHover = m_hoveredIndex;
    m_hoveredIndex = indexAt(event->pos());
    if (oldHover != m_hoveredIndex || m_assets.isEmpty()) update();
}

void GalleryWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

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
        QRect starRect(r.right() - 42, r.bottom() - 42, 38, 38);
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
