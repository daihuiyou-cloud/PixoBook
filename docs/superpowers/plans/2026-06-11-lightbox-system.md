# Lightbox System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a fullscreen QPainter overlay lightbox for immersive image preview.

**Architecture:** New `LightboxWidget` renders as a full-client-area QPainter overlay within MainWindow. It loads the current gallery asset list, displays images large with zoom/pan, and handles keyboard/mouse navigation. Communicates via signals for favorite toggling and close events. Uses a QTimer for overlay bar auto-fade.

**Tech Stack:** Qt5 Widgets + QPainter, C++17

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `src/ui/LightboxWidget.h` | Create | Class declaration, signal/slot definitions |
| `src/ui/LightboxWidget.cpp` | Create | Full QPainter overlay rendering, event handlers |
| `src/ui/MainWindow.h` | Modify | Add `LightboxWidget *m_lightbox` member |
| `src/ui/MainWindow.cpp` | Modify | Create lightbox, integrate signal wiring |

---

### Task 1: LightboxWidget Header

**Files:**
- Create: `src/ui/LightboxWidget.h`

- [ ] **Step 1: Write LightboxWidget.h**

```cpp
#ifndef LIGHTBOXWIDGET_H
#define LIGHTBOXWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPoint>
#include <QTimer>
#include "models/Asset.h"
#include "services/ImageCache.h"

class LightboxWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LightboxWidget(ImageCache *cache, QWidget *parent = nullptr);

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
    ImageCache *m_cache;

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
```

- [ ] **Step 2: Commit**

```bash
git add src/ui/LightboxWidget.h
git commit -m "feat: add LightboxWidget header"
```

---

### Task 2: LightboxWidget Implementation

**Files:**
- Create: `src/ui/LightboxWidget.cpp`

- [ ] **Step 1: Write LightboxWidget.cpp**

```cpp
#include "LightboxWidget.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QFileInfo>
#include <cmath>

LightboxWidget::LightboxWidget(ImageCache *cache, QWidget *parent)
    : QWidget(parent), m_cache(cache)
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
    // Leave space for overlay bars
    int topMargin = 40;
    int bottomMargin = 50;
    return r.adjusted(0, topMargin, 0, -bottomMargin);
}

QPointF LightboxWidget::imageCenter() const
{
    return imageRect().center() + m_panOffset;
}

void LightboxWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Background
    p.fillRect(rect(), QColor(0x1e, 0x1e, 0x1e));

    if (m_currentPixmap.isNull() || m_currentIndex < 0) {
        p.setPen(QColor(0x96, 0x96, 0x96));
        p.drawText(rect(), Qt::AlignCenter,
                   m_assets.isEmpty()
                       ? "No images"
                       : QString::fromUtf8("\xe6\x97\xa0\xe6\xb3\x95\xe5\x8a\xa0\xe8\xbd\xbd\xe5\x9b\xbe\xe7\x89\x87"));
        return;
    }

    // Calculate fit-zoom
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

    // Shadow under image
    QRect shadowRect(drawX - 4, drawY - 4, drawW + 8, drawH + 8);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 60));
    p.drawRoundedRect(shadowRect, 6, 6);

    // Image
    p.drawPixmap(drawX, drawY, drawW, drawH, m_currentPixmap);

    // Draw overlay bars if visible
    if (!m_overlayVisible) return;

    QFont f = p.font();
    f.setPointSize(11);
    p.setFont(f);

    // -- Top bar --
    QRect topBar(0, 0, width(), 40);
    p.fillRect(topBar, QColor(0, 0, 0, 180));

    p.setPen(QColor(0xcc, 0xcc, 0xcc));
    const Asset &asset = m_assets[m_currentIndex];
    QString fileName = QFileInfo(asset.filePath).fileName();
    p.drawText(topBar.adjusted(12, 0, 0, 0), Qt::AlignVCenter, fileName);

    // Close button
    m_closeBtnRect = QRect(width() - 36, 8, 24, 24);
    p.setPen(QColor(0xcc, 0xcc, 0xcc));
    QFont cf = p.font();
    cf.setPointSize(14);
    p.setFont(cf);
    p.drawText(m_closeBtnRect, Qt::AlignCenter, "\xc3\x97");

    // -- Bottom bar --
    QRect bottomBar(0, height() - 50, width(), 50);
    p.fillRect(bottomBar, QColor(0, 0, 0, 180));
    f.setPointSize(10);
    p.setFont(f);

    int cx = width() / 2;

    // Prev button
    m_prevBtnRect = QRect(cx - 200, height() - 44, 36, 36);
    bool canPrev = m_currentIndex > 0;
    p.setPen(canPrev ? QColor(0xcc, 0xcc, 0xcc) : QColor(0x50, 0x50, 0x50));
    QFont nf = p.font();
    nf.setPointSize(16);
    p.setFont(nf);
    p.drawText(m_prevBtnRect, Qt::AlignCenter, "\xe2\x97\x80");

    // Info text
    nf.setPointSize(10);
    p.setFont(nf);
    p.setPen(QColor(0xcc, 0xcc, 0xcc));
    QString info = QString("%1 x %2 | %3 | %4 KB")
                       .arg(asset.width)
                       .arg(asset.height)
                       .arg(asset.format.toUpper())
                       .arg(asset.fileSize / 1024);
    QRect infoRect(cx - 150, height() - 44, 300, 20);
    p.drawText(infoRect, Qt::AlignCenter, info);

    // Favorite button
    m_favBtnRect = QRect(cx + 160, height() - 44, 36, 36);
    p.setPen(asset.isFavorite ? QColor(0xff, 0xcc, 0x00) : QColor(0x96, 0x96, 0x96));
    nf.setPointSize(16);
    p.setFont(nf);
    p.drawText(m_favBtnRect, Qt::AlignCenter,
               asset.isFavorite ? "\xe2\x98\x85" : "\xe2\x98\x86");

    // Zoom level
    p.setPen(QColor(0x96, 0x96, 0x96));
    nf.setPointSize(10);
    p.setFont(nf);
    p.drawText(cx + 210, height() - 44, 60, 36, Qt::AlignVCenter,
               QString("%1%").arg((int)(m_zoom * 100)));

    // Position indicator
    p.setPen(QColor(0x96, 0x96, 0x96));
    p.drawText(cx - 280, height() - 44, 60, 36, Qt::AlignVCenter,
               QString("%1/%2").arg(m_currentIndex + 1).arg(m_assets.size()));

    // Next button
    m_nextBtnRect = QRect(cx + 280, height() - 44, 36, 36);
    bool canNext = m_currentIndex < m_assets.size() - 1;
    p.setPen(canNext ? QColor(0xcc, 0xcc, 0xcc) : QColor(0x50, 0x50, 0x50));
    nf.setPointSize(16);
    p.setFont(nf);
    p.drawText(m_nextBtnRect, Qt::AlignCenter, "\xe2\x96\xb6");

    // Top/bottom bar dividers
    p.setPen(QColor(0x3c, 0x3c, 0x3c));
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

void LightboxWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    bool overlayHit = false;

    // Close button
    if (m_overlayVisible && m_closeBtnRect.contains(event->pos())) {
        close();
        return;
    }

    // Navigation buttons
    if (m_overlayVisible && m_prevBtnRect.contains(event->pos())) {
        navigateTo(m_currentIndex - 1);
        return;
    }
    if (m_overlayVisible && m_nextBtnRect.contains(event->pos())) {
        navigateTo(m_currentIndex + 1);
        return;
    }

    // Favorite button
    if (m_overlayVisible && m_favBtnRect.contains(event->pos())) {
        if (m_currentIndex >= 0 && m_currentIndex < m_assets.size()) {
            Asset &a = m_assets[m_currentIndex];
            a.isFavorite = !a.isFavorite;
            emit favoriteToggled(a.id, a.isFavorite);
            update();
        }
        return;
    }

    // Click on image area — left/right navigation or pan
    QRect imgArea = imageRect();
    if (imgArea.contains(event->pos())) {
        if (m_zoom > 1.05) {
            m_isPanning = true;
            m_lastPanPos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            overlayHit = true;
        } else {
            // Left/right half navigation
            if (event->pos().x() < imgArea.center().x())
                navigateTo(m_currentIndex - 1);
            else
                navigateTo(m_currentIndex + 1);
            return;
        }
    }

    if (!overlayHit) {
        // Click outside image area — close
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

    // Show overlay bars on any mouse movement
    if (!m_overlayVisible) {
        m_overlayVisible = true;
        update();
    }
    m_overlayTimer->start();

    m_lastMousePos = event->pos();

    // Update cursor
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

    double factor = event->angleDelta().y() > 0 ? 1.1 : 0.9;
    double newZoom = m_zoom * factor;
    newZoom = qBound(0.1, newZoom, 10.0);

    // Zoom centered on mouse position
    double fitScale = qMin(
        (double)imgArea.width() / m_currentPixmap.width(),
        (double)imgArea.height() / m_currentPixmap.height()
    );

    // Mouse position relative to image center before zoom
    double relX = event->pos().x() - (imgArea.center().x() + m_panOffset.x());
    double relY = event->pos().y() - (imgArea.center().y() + m_panOffset.y());

    // Scale relative position inversely with zoom change
    double ratio = m_zoom / newZoom;
    m_panOffset.setX(event->pos().x() - imgArea.center().x() - relX * ratio);
    m_panOffset.setY(event->pos().y() - imgArea.center().y() - relY * ratio);

    m_zoom = newZoom;
    update();
}
```

- [ ] **Step 2: Commit**

```bash
git add src/ui/LightboxWidget.cpp
git commit -m "feat: add LightboxWidget implementation"
```

---

### Task 3: Integrate Lightbox into MainWindow

**Files:**
- Modify: `src/ui/MainWindow.h` — add member
- Modify: `src/ui/MainWindow.cpp` — create lightbox, wire signals

- [ ] **Step 1: Update MainWindow.h**

Add `#include "ui/LightboxWidget.h"` after existing includes:

```cpp
#include "ui/GalleryWidget.h"
#include "ui/DetailPanel.h"
#include "ui/SidebarWidget.h"
#include "ui/SearchBar.h"
#include "ui/LightboxWidget.h"
```

Add member after the existing member declarations:

```cpp
    QStringList m_folders;
    int m_activeTagId = -1;
    LightboxWidget *m_lightbox;
```

- [ ] **Step 2: Update MainWindow::setupUI()**

After creating the gallery widget and setting up the splitter, create the lightbox:

```cpp
    connect(m_searchBar, &SearchBar::filterChanged, this, &MainWindow::onFilterChanged);
    connect(m_searchBar, &SearchBar::thumbnailSizeChanged, m_gallery, &GalleryWidget::setThumbnailSize);

    // Lightbox — overlay on top of everything
    m_lightbox = new LightboxWidget(m_cache, this);
    m_lightbox->lower(); // starts below everything

    connect(m_gallery, &GalleryWidget::assetDoubleClicked, this, [this](const Asset &asset) {
        int idx = m_gallery->selectedAssetIndex();
        if (idx < 0) return;
        m_lightbox->show(m_gallery->allAssets(), idx);
    });
    connect(m_lightbox, &LightboxWidget::closed, this, [this]() {
        m_gallery->setFocus();
    });
    connect(m_lightbox, &LightboxWidget::favoriteToggled, this, [this](const QString &assetId, bool isFavorite) {
        m_db->updateAssetFavorite(assetId, isFavorite);
        loadAssets();
    });
```

- [ ] **Step 3: Add needed helpers to GalleryWidget.h**

Add two methods to GalleryWidget public interface:

```cpp
    int selectedAssetIndex() const;
    QVector<Asset> allAssets() const;
```

- [ ] **Step 3b: Add Space/Enter trigger in GalleryWidget::keyPressEvent**

In GalleryWidget::keyPressEvent, after the Home/End handling and before the final `QWidget::keyPressEvent(event)`, add:

```cpp
    } else if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) {
        if (!m_selectedAsset.id.isEmpty())
            emit assetDoubleClicked(m_selectedAsset);
    }

    QWidget::keyPressEvent(event);
```

This makes Space/Enter open the lightbox on the selected asset.

- [ ] **Step 4: Add helpers to GalleryWidget.cpp**

```cpp
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

QVector<Asset> GalleryWidget::allAssets() const
{
    return m_assets;
}
```

- [ ] **Step 5: Build and verify**

```bash
cd D:\workspace\Artiface\build
cmake .. -DQt5_DIR="D:/Qt5.13.2/5.13.2/msvc2017_64/lib/cmake/Qt5"
cmake --build . --config Release -- /m
```

Expected: Build succeeds, no errors.

- [ ] **Step 6: Run tests**

```powershell
$env:Path += ";D:\Qt5.13.2\5.13.2\msvc2017_64\bin"
cd D:\workspace\Artiface\build\Release
.\tst_models.exe
.\tst_database.exe
.\tst_parsers.exe
.\tst_filestorage.exe
```

Expected: All 4 test suites pass.

- [ ] **Step 7: Commit**

```bash
git add src/ui/MainWindow.h src/ui/MainWindow.cpp \
       src/ui/GalleryWidget.h src/ui/GalleryWidget.cpp
git commit -m "feat: integrate lightbox into MainWindow"
```
