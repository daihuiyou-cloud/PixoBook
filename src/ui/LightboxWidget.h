#ifndef LIGHTBOXWIDGET_H
#define LIGHTBOXWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPoint>
#include <QTimer>
#include "models/Asset.h"
#include "core/IImageCache.h"

class LightboxWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LightboxWidget(IImageCache *cache, QWidget *parent = nullptr);

    void show(const QVector<Asset> &assets, int startIndex);
    void close();

signals:
    void closed();
    void favoriteToggled(const QString &assetId, bool isFavorite);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void loadCurrentImage();
    void resetView();
    void navigateTo(int index);
    QRect imageRect() const;
    QPointF imageCenter() const;

    QVector<Asset> m_assets;
    int m_currentIndex = -1;
    QPixmap m_currentPixmap;
    IImageCache *m_cache;

    double m_zoom = 1.0;
    QPointF m_panOffset;
    bool m_isPanning = false;
    QPoint m_lastPanPos;
    QPoint m_lastMousePos;
    bool m_overlayVisible = true;
    QTimer *m_overlayTimer;

    QRect m_closeBtnRect;
    QRect m_prevBtnRect;
    QRect m_nextBtnRect;
    QRect m_favBtnRect;
};

#endif
