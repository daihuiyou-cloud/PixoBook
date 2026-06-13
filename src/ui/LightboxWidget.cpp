#include "LightboxWidget.h"
#include <QPainter>
#include <QKeyEvent>
#include "ui/Codicon.h"
#include "ui/ColorConstants.h"
#include "ui/VisualConstants.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFileInfo>

LightboxWidget::LightboxWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    hide();

    m_overlayTimer = new QTimer(this);
    m_overlayTimer->setSingleShot(true);
    m_overlayTimer->setInterval(2000);
    connect(m_overlayTimer, &QTimer::timeout, this, [this]() {
        m_overlayVisible = false;
        update();
    });
}

void LightboxWidget::show(const QVector<Asset> &assets, int startIndex)
{
    m_assets = assets;
    m_currentIndex = qBound(0, startIndex, assets.size() - 1);
    resetView();
    loadCurrentImage();
    if (parentWidget()) {
        resize(parentWidget()->size());
        move(0, 0);
    }
    setVisible(true);
    raise();
    setFocus();
}

void LightboxWidget::close()
{
    hide();
    emit closed();
}

void LightboxWidget::loadCurrentImage()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_assets.size()) {
        m_currentPixmap = QPixmap();
        return;
    }
    const Asset &asset = m_assets[m_currentIndex];
    if (QFileInfo::exists(asset.filePath)) {
        m_currentPixmap = QPixmap(asset.filePath);
    } else {
        m_currentPixmap = QPixmap();
    }
}

void LightboxWidget::resetView()
{
    m_zoom = 1.0;
    m_panOffset = {0, 0};
    m_isPanning = false;
    m_overlayVisible = true;
    m_overlayTimer->start();
}

void LightboxWidget::navigateTo(int index)
{
    if (index < 0 || index >= m_assets.size()) return;
    m_currentIndex = index;
    resetView();
    loadCurrentImage();
    update();
}

QRect LightboxWidget::imageRect() const
{
    QRect r = rect();
    int topMargin = 40;
    int bottomMargin = 50;
    return r.adjusted(0, topMargin, 0, -bottomMargin);
}

void LightboxWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    p.fillRect(rect(), Color::BG_DARKEST);

    if (m_currentPixmap.isNull() || m_currentIndex < 0 || m_currentIndex >= m_assets.size()) {
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(rect(), Qt::AlignCenter,
                   m_assets.isEmpty()
                       ? QStringLiteral("No images")
                       : QStringLiteral("无法加载图片"));
        return;
    }

    QRect imgRect = imageRect();
    double fitScale = qMin(
        (double)imgRect.width() / m_currentPixmap.width(),
        (double)imgRect.height() / m_currentPixmap.height()
    );
    double scale = fitScale * m_zoom;

    int drawW = (int)(m_currentPixmap.width() * scale);
    int drawH = (int)(m_currentPixmap.height() * scale);

    int drawX = imgRect.center().x() - drawW / 2 + (int)m_panOffset.x();
    int drawY = imgRect.center().y() - drawH / 2 + (int)m_panOffset.y();

    QRect shadowRect(drawX - 4, drawY - 4, drawW + 8, drawH + 8);
    p.setPen(Qt::NoPen);
    p.setBrush(Color::OVERLAY_SHADOW);
    p.drawRoundedRect(shadowRect, 6, 6);

    p.drawPixmap(drawX, drawY, drawW, drawH, m_currentPixmap);

    if (!m_overlayVisible) {
        m_closeBtnRect = QRect();
        m_prevBtnRect = QRect();
        m_nextBtnRect = QRect();
        m_favBtnRect = QRect();
        return;
    }

    QFont f = p.font();
    f.setPixelSize(Visual::FontControl);
    p.setFont(f);

    // Top bar
    QRect topBar(0, 0, width(), 40);
    p.fillRect(topBar, Color::OVERLAY_BG);

    p.setPen(Color::TEXT_PRIMARY);
    const Asset &asset = m_assets[m_currentIndex];
    QString fileName = QFileInfo(asset.filePath).fileName();
    p.drawText(topBar.adjusted(12, 0, 0, 0), Qt::AlignVCenter, fileName);

    m_closeBtnRect = QRect(width() - 36, 8, 24, 24);
    Codicon::draw(p, "close", m_closeBtnRect, Color::TEXT_PRIMARY, 18);

    // Bottom bar
    QRect bottomBar(0, height() - 50, width(), 50);
    p.fillRect(bottomBar, Color::OVERLAY_BG);
    f.setPixelSize(Visual::FontMeta);
    p.setFont(f);

    int cx = width() / 2;

    m_prevBtnRect = QRect(cx - 200, height() - 44, 36, 36);
    QColor prevClr = (m_currentIndex > 0) ? Color::TEXT_PRIMARY : Color::TEXT_DISABLED;
    Codicon::draw(p, "chevron-left", m_prevBtnRect, prevClr, 18);

    p.setPen(Color::TEXT_PRIMARY);
    QString info = QString("%1 x %2 | %3 | %4 KB")
                       .arg(asset.width)
                       .arg(asset.height)
                       .arg(asset.format.toUpper())
                       .arg(asset.fileSize / 1024);
    QRect infoRect(cx - 150, height() - 44, 300, 20);
    p.drawText(infoRect, Qt::AlignCenter, info);

    m_favBtnRect = QRect(cx + 160, height() - 44, 36, 36);
    QColor favClr = asset.isFavorite ? Color::FAVORITE_ON : Color::TEXT_SECONDARY;
    Codicon::draw(p, "star", m_favBtnRect, favClr, 18);

    p.setPen(Color::TEXT_SECONDARY);
    p.drawText(cx + 210, height() - 44, 60, 36, Qt::AlignVCenter,
               QString("%1%").arg((int)(m_zoom * 100)));

    p.setPen(Color::TEXT_SECONDARY);
    p.drawText(cx - 280, height() - 44, 60, 36, Qt::AlignVCenter,
               QString("%1/%2").arg(m_currentIndex + 1).arg(m_assets.size()));

    m_nextBtnRect = QRect(cx + 280, height() - 44, 36, 36);
    QColor nextClr = (m_currentIndex < m_assets.size() - 1) ? Color::TEXT_PRIMARY : Color::TEXT_DISABLED;
    Codicon::draw(p, "chevron-right", m_nextBtnRect, nextClr, 18);

    p.setPen(Color::BORDER);
    p.drawLine(0, 40, width(), 40);
    p.drawLine(0, height() - 50, width(), height() - 50);
}

void LightboxWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        close();
        break;
    case Qt::Key_Left:
    case Qt::Key_Up:
        navigateTo(m_currentIndex - 1);
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
        navigateTo(m_currentIndex + 1);
        break;
    case Qt::Key_Home:
        navigateTo(0);
        break;
    case Qt::Key_End:
        navigateTo(m_assets.size() - 1);
        break;
    case Qt::Key_F:
        if (m_currentIndex >= 0 && m_currentIndex < m_assets.size()) {
            Asset &a = m_assets[m_currentIndex];
            a.isFavorite = !a.isFavorite;
            emit favoriteToggled(a.id, a.isFavorite);
            update();
        }
        break;
    case Qt::Key_R:
        resetView();
        update();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void LightboxWidget::resizeEvent(QResizeEvent *)
{
    if (parentWidget())
        resize(parentWidget()->size());
}

void LightboxWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    bool overlayHit = false;

    if (m_overlayVisible && m_closeBtnRect.contains(event->pos())) {
        close();
        return;
    }

    if (m_overlayVisible && m_prevBtnRect.contains(event->pos())) {
        navigateTo(m_currentIndex - 1);
        return;
    }
    if (m_overlayVisible && m_nextBtnRect.contains(event->pos())) {
        navigateTo(m_currentIndex + 1);
        return;
    }

    if (m_overlayVisible && m_favBtnRect.contains(event->pos())) {
        if (m_currentIndex >= 0 && m_currentIndex < m_assets.size()) {
            Asset &a = m_assets[m_currentIndex];
            a.isFavorite = !a.isFavorite;
            emit favoriteToggled(a.id, a.isFavorite);
            update();
        }
        return;
    }

    QRect imgArea = imageRect();
    if (imgArea.contains(event->pos())) {
        if (m_zoom > 1.05) {
            m_isPanning = true;
            m_lastPanPos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            overlayHit = true;
        } else {
            int margin = imgArea.width() / 5;
            if (event->pos().x() < imgArea.left() + margin)
                navigateTo(m_currentIndex - 1);
            else if (event->pos().x() > imgArea.right() - margin)
                navigateTo(m_currentIndex + 1);
            else
                overlayHit = true;
            return;
        }
    }

    if (!overlayHit) {
        close();
    }
}

void LightboxWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_panOffset += delta;
        m_lastPanPos = event->pos();
        update();
    }

    if (!m_overlayVisible) {
        m_overlayVisible = true;
        update();
    }
    m_overlayTimer->start();

    QRect imgArea = imageRect();
    if (imgArea.contains(event->pos()) && m_zoom > 1.05 && !m_isPanning) {
        setCursor(Qt::OpenHandCursor);
    } else if (!m_isPanning) {
        setCursor(Qt::ArrowCursor);
    }
}

void LightboxWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
}

void LightboxWidget::wheelEvent(QWheelEvent *event)
{
    QRect imgArea = imageRect();
    if (!imgArea.contains(event->pos())) return;

    QPoint pixelDelta = event->pixelDelta();
    QPoint angleDelta = event->angleDelta();
    double delta = pixelDelta.isNull() ? angleDelta.y() : pixelDelta.y();
    double factor = delta > 0 ? 1.1 : 0.9;
    double newZoom = m_zoom * factor;
    newZoom = qBound(0.1, newZoom, 10.0);

    double fitScale = qMin(
        (double)imgArea.width() / m_currentPixmap.width(),
        (double)imgArea.height() / m_currentPixmap.height()
    );

    double relX = event->pos().x() - (imgArea.center().x() + m_panOffset.x());
    double relY = event->pos().y() - (imgArea.center().y() + m_panOffset.y());

    double ratio = m_zoom / newZoom;
    m_panOffset.setX(event->pos().x() - imgArea.center().x() - relX * ratio);
    m_panOffset.setY(event->pos().y() - imgArea.center().y() - relY * ratio);

    m_zoom = newZoom;
    update();
}
