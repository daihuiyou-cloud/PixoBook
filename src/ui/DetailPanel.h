#ifndef DETAILPANEL_H
#define DETAILPANEL_H

#include <QWidget>
#include <QPlainTextEdit>
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
    void previewRequested(const QString &assetId);
    void promptSaved(const QString &assetId, const QString &newPrompt);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void startPromptEdit();
    void finishPromptEdit(bool accepted);

    Asset m_asset;
    Metadata m_metadata;
    QVector<Tag> m_tags;

    QPlainTextEdit *m_promptEditor = nullptr;
    bool m_isEditingPrompt = false;

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
    QRect m_copyPromptRect;
    QRect m_promptHeaderRect;
    QRect m_promptContentRect;
    QRect m_fileInfoHeaderRect;
    QRect m_metadataHeaderRect;
    QRect m_tagsHeaderRect;
    QRect m_copyFileNameRect;
    QRect m_openFolderRect;
    QRect m_openPreviewRect;
    QVector<QPair<int, QRect>> m_tagRects;
    bool m_addTagHovered = false;
    bool m_copyPromptHovered = false;
    bool m_closeBtnHovered = false;
    bool m_copyFileNameHovered = false;
    bool m_openFolderHovered = false;
    bool m_openPreviewHovered = false;
    bool m_promptHeaderHovered = false;
    bool m_fileInfoHeaderHovered = false;
    bool m_metadataHeaderHovered = false;
    bool m_tagsHeaderHovered = false;

    IImageCache *m_cache;

    QRect imageArea() const;
    int drawImage(QPainter &p);
    int drawAssetSummary(QPainter &p, int y);
    void drawSummaryAction(QPainter &p, const QRect &rect, const QString &icon,
                           const QString &text, bool hovered, bool enabled);
    int drawFileInfo(QPainter &p, int y);
    int drawMetadataSection(QPainter &p, int y);
    int drawTagsSection(QPainter &p, int y);
    void drawSectionDivider(QPainter &p, int y);
    void clampScrollOffset();
    void drawScrollIndicator(QPainter &p);
    void drawField(QPainter &p, int x, int &y, const QString &label, const QString &value, int labelW = 88);
};

#endif
