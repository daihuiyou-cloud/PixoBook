#ifndef DETAILPANEL_H
#define DETAILPANEL_H

#include <QWidget>
#include "models/Asset.h"
#include "models/Metadata.h"
#include "models/Tag.h"
#include "core/IImageCache.h"

class DetailPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DetailPanel(IImageCache *cache, QWidget *parent = nullptr);

    void showAsset(const Asset &asset, const Metadata &metadata, const QVector<Tag> &tags);
    void clear();
    QString currentAssetId() const { return m_asset.id; }

signals:
    void favoriteToggled(const QString &assetId, bool isFavorite);
    void tagRemoved(const QString &assetId, int tagId);
    void tagAddRequested(const QString &assetId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    Asset m_asset;
    Metadata m_metadata;
    QVector<Tag> m_tags;

    QPixmap m_fullImage;
    double m_zoom = 1.0;
    QPoint m_panOffset;
    bool m_isPanning = false;
    QPoint m_lastPanPos;

    int m_scrollOffset = 0;
    int m_maxScrollOffset = 0;

    bool m_promptExpanded = true;
    bool m_fileInfoExpanded = true;
    bool m_metadataExpanded = true;
    bool m_tagsExpanded = true;

    QRect m_favStarRect;
    QRect m_addTagRect;
    QRect m_closeBtnRect;
    QRect m_promptHeaderRect;
    QRect m_fileInfoHeaderRect;
    QRect m_metadataHeaderRect;
    QRect m_tagsHeaderRect;
    QVector<QPair<int, QRect>> m_tagRects;

    IImageCache *m_cache;

    QRect imageArea() const;
    QRect metadataArea() const;
    int drawImage(QPainter &p);
    int drawFileInfo(QPainter &p, int y);
    int drawMetadataSection(QPainter &p, int y);
    int drawTagsSection(QPainter &p, int y);
    void drawSectionDivider(QPainter &p, int y);
    void drawField(QPainter &p, int x, int &y, const QString &label, const QString &value, int labelW = 80);
};

#endif
