#ifndef GALLERYWIDGET_H
#define GALLERYWIDGET_H

#include <QWidget>
#include <QVector>
#include <QSet>
#include "models/Asset.h"
#include "core/IImageCache.h"

class GalleryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GalleryWidget(IImageCache *cache, QWidget *parent = nullptr);

    void setAssets(const QVector<Asset> &assets);
    void appendAssets(const QVector<Asset> &assets);
    void clearAssets();
    Asset selectedAsset() const { return m_selectedAsset; }
    QVector<Asset> selectedAssets() const;
    int assetCount() const { return m_assets.size(); }
    int selectedAssetIndex() const;
    QVector<Asset> allAssets() const;

    void setThumbnailSize(int size);
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
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void layoutItems();
    QRect itemRect(int index) const;
    int indexAt(const QPoint &pos) const;
    void navigateTo(int index);

    int m_thumbSize = 180;
    static constexpr int kPadding = 10;
    int m_itemWidth() const { return m_thumbSize + kPadding * 2; }
    int m_itemHeight() const { return m_thumbSize + kPadding * 2 + 22; }

    QVector<Asset> m_assets;
    QSet<int> m_selectedIndices;
    Asset m_selectedAsset;
    int m_hoveredIndex = -1;
    int m_scrollOffset = 0;
    int m_totalHeight = 0;
    int m_columns = 4;
    QString m_searchKeyword;
    IImageCache *m_cache;
};

#endif
