#ifndef LIGHTBOXWIDGET_H
#define LIGHTBOXWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPoint>
#include <QTimer>
#include "models/Asset.h"

class LightboxWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LightboxWidget(QWidget *parent = nullptr);

    void show(const QVector<Asset> &assets, int startIndex);
    void close();

signals:
    void closed();
    void favoriteToggled(const QString &assetId, bool isFavorite);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
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

    QVector<Asset> m_assets;
    int m_currentIndex = -1;
    QPixmap m_currentPixmap;

    double m_zoom = 1.0;
    QPointF m_panOffset;
    bool m_isPanning = false;
    QPoint m_lastPanPos;
    bool m_overlayVisible = true;
    QTimer *m_overlayTimer;

    QRect m_closeBtnRect;
    QRect m_prevBtnRect;
    QRect m_nextBtnRect;
    QRect m_favBtnRect;
    QRect m_zoomOutBtnRect;
    QRect m_resetZoomBtnRect;
    QRect m_zoomInBtnRect;

    QFont m_fontControl;
    QFont m_fontMeta;
    void rebuildInfoStrings();
    QString m_counterText;
    QString m_infoText;
    QString m_zoomText;
};

#endif
