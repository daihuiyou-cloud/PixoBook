#include "LightboxWidget.h"
#include <QCursor>
#include <QFileInfo>
#include <QImageReader>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include "ui/Codicon.h"
#include "ui/ColorConstants.h"
#include "ui/VisualConstants.h"

namespace {
void drawToolbarButton(QPainter &p, const QRect &rect, const QString &icon, const QColor &color, bool hovered)
{
    p.setBrush(hovered ? Color::BG_BUTTON_HOVER : QColor(0xff, 0xff, 0xff, 12));
    p.setPen(QPen(hovered ? Color::BORDER_SUBTLE : Color::BORDER_FAINT, 1));
    p.drawRoundedRect(rect, Visual::RadiusSmall, Visual::RadiusSmall);
    p.setBrush(Qt::NoBrush);
    Codicon::draw(p, icon, rect, color, 16);
}
}

LightboxWidget::LightboxWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    hide();

    m_overlayTimer = new QTimer(this);
    m_overlayTimer->setSingleShot(true);
    m_overlayTimer->setInterval(3000);
    connect(m_overlayTimer, &QTimer::timeout, this, [this]() {
        m_overlayVisible = false;
        update();
    });

    QFont base = font();
    m_fontControl = base; m_fontControl.setPixelSize(Visual::FontControl);
    m_fontMeta = base; m_fontMeta.setPixelSize(Visual::FontMeta);
}

void LightboxWidget::rebuildInfoStrings()
{
    m_counterText = QStringLiteral("%1/%2").arg(m_currentIndex + 1).arg(m_assets.size());
    if (m_currentIndex >= 0 && m_currentIndex < m_assets.size()) {
        const auto &a = m_assets[m_currentIndex];
        m_infoText = QStringLiteral("%1 x %2 | %3 | %4 KB")
                         .arg(a.width).arg(a.height).arg(a.format.toUpper()).arg(a.fileSize / 1024);
    }
    m_zoomText = QStringLiteral("%1%").arg(static_cast<int>(m_zoom * 100));
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
    if (!QFileInfo::exists(asset.filePath)) {
        m_currentPixmap = QPixmap();
        return;
    }
    QImageReader reader(asset.filePath);
    QSize imgSize = reader.size();
    if (imgSize.isValid()) {
        QSize maxSize(1920, 1080);
        if (imgSize.width() > maxSize.width() || imgSize.height() > maxSize.height())
            reader.setScaledSize(imgSize.scaled(maxSize, Qt::KeepAspectRatio));
    }
    m_currentPixmap = QPixmap::fromImage(reader.read());
}

void LightboxWidget::resetView()
{
    m_zoom = 1.0;
    m_panOffset = {0, 0};
    m_isPanning = false;
    m_overlayVisible = true;
    m_overlayTimer->start();
    rebuildInfoStrings();
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
                   m_assets.isEmpty() ? QStringLiteral("No images") : tr("无法加载图片"));
        return;
    }

    QRect imgRect = imageRect();
    double fitScale = qMin(static_cast<double>(imgRect.width()) / m_currentPixmap.width(),
                           static_cast<double>(imgRect.height()) / m_currentPixmap.height());
    double scale = fitScale * m_zoom;

    int drawW = static_cast<int>(m_currentPixmap.width() * scale);
    int drawH = static_cast<int>(m_currentPixmap.height() * scale);
    int drawX = imgRect.center().x() - drawW / 2 + static_cast<int>(m_panOffset.x());
    int drawY = imgRect.center().y() - drawH / 2 + static_cast<int>(m_panOffset.y());

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
        m_zoomOutBtnRect = QRect();
        m_resetZoomBtnRect = QRect();
        m_zoomInBtnRect = QRect();
        return;
    }

    const QPoint localCursor = mapFromGlobal(QCursor::pos());
    const Asset &asset = m_assets[m_currentIndex];

    p.setFont(m_fontControl);

    QRect topBar(0, 0, width(), 40);
    p.fillRect(topBar, Color::OVERLAY_BG);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(topBar.adjusted(12, 0, -48, 0), Qt::AlignVCenter | Qt::AlignLeft, QFileInfo(asset.filePath).fileName());

    m_closeBtnRect = QRect(width() - 36, 8, 24, 24);
    Codicon::draw(p, "close", m_closeBtnRect, Color::TEXT_PRIMARY, 18);

    QRect bottomBar(0, height() - 50, width(), 50);
    p.fillRect(bottomBar, Color::OVERLAY_BG);

    int cx = width() / 2;

    p.setFont(m_fontMeta);
    p.setPen(Color::TEXT_SECONDARY);
    p.drawText(QRect(cx - 320, height() - 43, 60, 34), Qt::AlignVCenter | Qt::AlignLeft,
               m_counterText);

    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(QRect(cx - 244, height() - 43, 200, 34), Qt::AlignLeft | Qt::AlignVCenter, m_infoText);

    m_zoomOutBtnRect = QRect(cx - 28, height() - 43, 34, 34);
    m_resetZoomBtnRect = QRect(cx + 12, height() - 43, 58, 34);
    m_zoomInBtnRect = QRect(cx + 76, height() - 43, 34, 34);
    drawToolbarButton(p, m_zoomOutBtnRect, "zoom-out", Color::TEXT_PRIMARY, m_zoomOutBtnRect.contains(localCursor));
    drawToolbarButton(p, m_zoomInBtnRect, "zoom-in", Color::TEXT_PRIMARY, m_zoomInBtnRect.contains(localCursor));

    bool resetHovered = m_resetZoomBtnRect.contains(localCursor);
    p.setBrush(resetHovered ? Color::BG_BUTTON_HOVER : QColor(0xff, 0xff, 0xff, 12));
    p.setPen(QPen(resetHovered ? Color::BORDER_SUBTLE : Color::BORDER_FAINT, 1));
    p.drawRoundedRect(m_resetZoomBtnRect, Visual::RadiusSmall, Visual::RadiusSmall);
    p.setBrush(Qt::NoBrush);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(m_resetZoomBtnRect, Qt::AlignCenter, m_zoomText);

    m_prevBtnRect = QRect(cx + 96, height() - 43, 34, 34);
    QColor prevClr = (m_currentIndex > 0) ? Color::TEXT_PRIMARY : Color::TEXT_DISABLED;
    drawToolbarButton(p, m_prevBtnRect, "chevron-left", prevClr, m_prevBtnRect.contains(localCursor));

    m_favBtnRect = QRect(cx + 144, height() - 43, 34, 34);
    QColor favClr = asset.isFavorite ? Color::FAVORITE_ON : Color::TEXT_SECONDARY;
    drawToolbarButton(p, m_favBtnRect, "star", favClr, m_favBtnRect.contains(localCursor));

    m_nextBtnRect = QRect(cx + 192, height() - 43, 34, 34);
    QColor nextClr = (m_currentIndex < m_assets.size() - 1) ? Color::TEXT_PRIMARY : Color::TEXT_DISABLED;
    drawToolbarButton(p, m_nextBtnRect, "chevron-right", nextClr, m_nextBtnRect.contains(localCursor));

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
    if (m_overlayVisible && m_zoomOutBtnRect.contains(event->pos())) {
        m_zoom = qBound(0.1, m_zoom * 0.85, 10.0);
        rebuildInfoStrings();
        update();
        return;
    }
    if (m_overlayVisible && m_zoomInBtnRect.contains(event->pos())) {
        m_zoom = qBound(0.1, m_zoom * 1.15, 10.0);
        rebuildInfoStrings();
        update();
        return;
    }
    if (m_overlayVisible && m_resetZoomBtnRect.contains(event->pos())) {
        resetView();
        update();
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

    if (!overlayHit)
        close();
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
    if (imgArea.contains(event->pos()) && m_zoom > 1.05 && !m_isPanning)
        setCursor(Qt::OpenHandCursor);
    else if (!m_isPanning)
        setCursor(Qt::ArrowCursor);
}

void LightboxWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
}

void LightboxWidget::wheelEvent(QWheelEvent *event)
{
    QRect imgArea = imageRect();
    if (!imgArea.contains(event->pos()) || m_currentPixmap.isNull()) return;

    QPoint pixelDelta = event->pixelDelta();
    QPoint angleDelta = event->angleDelta();
    double delta = pixelDelta.isNull() ? angleDelta.y() : pixelDelta.y();
    double factor = delta > 0 ? 1.1 : 0.9;
    double newZoom = m_zoom * factor;
    newZoom = qBound(0.1, newZoom, 10.0);

    double relX = event->pos().x() - (imgArea.center().x() + m_panOffset.x());
    double relY = event->pos().y() - (imgArea.center().y() + m_panOffset.y());
    double ratio = m_zoom / newZoom;
    m_panOffset.setX(event->pos().x() - imgArea.center().x() - relX * ratio);
    m_panOffset.setY(event->pos().y() - imgArea.center().y() - relY * ratio);

    m_zoom = newZoom;
    rebuildInfoStrings();
    update();
}
