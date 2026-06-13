#ifndef GALLERYWIDGET_H
#define GALLERYWIDGET_H

#include <QWidget>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QFont>
#include <QFontMetrics>
#include "ui/VisualConstants.h"
#include "models/Asset.h"
#include "core/IImageCache.h"

class QPainter;

class GalleryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GalleryWidget(IImageCache *cache, QWidget *parent = nullptr);

    void setAssets(const QVector<Asset> &assets);
    void setAssets(const QVector<Asset> &assets, int totalCount);
    void appendPage(const QVector<Asset> &assets, int totalCount);
    void appendAssets(const QVector<Asset> &assets);
    void clearAssets();
    Asset selectedAsset() const { return m_selectedAsset; }
    QVector<Asset> selectedAssets() const;
    int assetCount() const { return m_assets.size(); }
    int totalAssetCount() const { return m_totalAssetCount; }
    int selectedAssetIndex() const;
    QVector<Asset> allAssets() const;

    void setThumbnailSize(int size);
    int thumbnailSize() const { return m_thumbSize; }
    void setSearchKeyword(const QString &keyword) { m_searchKeyword = keyword; update(); }
    QString searchKeyword() const { return m_searchKeyword; }

public slots:
    void onThumbnailReady(const QString &filePath, const QSize &size, const QPixmap &pixmap);

signals:
    void assetSelected(const Asset &asset);
    void assetDoubleClicked(const Asset &asset);
    void filesDropped(const QStringList &paths);
    void deleteRequested(const QVector<Asset> &assets);
    void favoriteToggled(const QString &assetId, bool isFavorite);
    void tagAddRequested(const QVector<QString> &assetIds);
    void importFolderRequested();
    void importFilesRequested();
    void loadMoreRequested(int offset, int limit);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool event(QEvent *event) override;

private:
    void layoutItems();
    QRect itemRect(int index) const;
    int indexAt(const QPoint &pos) const;
    void navigateTo(int index);
    void drawEmptyState(QPainter &p);
    QRect emptyFolderButtonRect() const;
    QRect emptyFilesButtonRect() const;
    void drawBatchToolbar(QPainter &p);
    void clearSelection();
    void checkLoadMore();
    void prefetchFileExistence(const QVector<Asset> &assets);

    int m_thumbSize = 180;
    static constexpr int kPadding = Visual::GalleryCardPadding;
    static constexpr int kGap = Visual::GalleryCardGap;
    static constexpr int kLabelHeight = 40;
    static constexpr int kPageSize = 200;
    int m_totalAssetCount = 0;
    int m_itemWidth() const { return m_thumbSize + kPadding * 2; }
    int m_itemHeight() const { return m_thumbSize + kPadding * 2 + kLabelHeight; }

    QVector<Asset> m_assets;
    QSet<int> m_selectedIndices;
    Asset m_selectedAsset;
    int m_hoveredIndex = -1;
    int m_lastClickedIndex = -1;
    int m_scrollOffset = 0;
    int m_totalHeight = 0;
    int m_columns = 4;
    QString m_searchKeyword;
    bool m_isDragOver = false;
    QSet<QString> m_requestedThumbnails;
    QHash<QString, bool> m_fileExistsCache;
    QRect m_batchTagRect;
    QRect m_batchDeleteRect;
    QRect m_batchClearRect;
    IImageCache *m_cache;

    QFont m_labelFont;
    QFontMetrics m_labelFm{m_labelFont};
    QFont m_metaFont;
    QFontMetrics m_metaFm{m_metaFont};
};

#endif
