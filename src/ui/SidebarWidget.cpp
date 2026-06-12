#include "SidebarWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFontMetrics>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>
#include "ui/Codicon.h"
#include "ui/ColorConstants.h"

SidebarWidget::SidebarWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFixedWidth(220);
    setMinimumHeight(200);
    setFocusPolicy(Qt::StrongFocus);
}

void SidebarWidget::setTags(const QVector<Tag> &tags)
{
    m_tags = tags;
    updateLayout();
    update();
}

void SidebarWidget::setFolders(const QStringList &folders)
{
    m_folders = folders;
    updateLayout();
    update();
}

void SidebarWidget::setActiveFolder(const QString &folder)
{
    m_activeFolder = folder;
    m_activeTagId = -1;
    update();
}

void SidebarWidget::setActiveTag(int tagId)
{
    m_activeTagId = tagId;
    m_activeFolder.clear();
    update();
}

void SidebarWidget::updateLayout()
{
    m_sectionTagY = (m_folders.isEmpty() ? 0 : kSectionHeight + m_folders.size() * kItemHeight + 8);
}

void SidebarWidget::drawFolderIcon(QPainter &p, const QRect &r, bool hovered, bool active) const
{
    QColor fc = active ? Color::TEXT_PRIMARY : (hovered ? QColor(0xc0, 0xa0, 0x60) : Color::TEXT_SECONDARY);
    Codicon::draw(p, "folder", QRect(r.x() + 4, r.y(), 20, r.height()), fc, 16);
}

void SidebarWidget::drawTagDot(QPainter &p, const QPoint &center, const QColor &color) const
{
    Codicon::draw(p, "tag", QRect(center.x() - 8, center.y() - 8, 16, 16), color, 14);
}

void SidebarWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), Color::BG_DARK);

    // -- Folders section --
    if (!m_folders.isEmpty()) {
        int sectionY = 0;
        QRect sectionRect(0, sectionY, width(), kSectionHeight);
        p.setPen(Color::TEXT_SECONDARY);
        QFont sf = p.font();
        sf.setBold(false);
        p.setFont(sf);
        QString folderHeader = Codicon::icon(m_foldersExpanded ? "chevron-down" : "chevron-right")
                             + "  " + QStringLiteral("文件夹");
        p.drawText(sectionRect.adjusted(12, 0, 0, 0), Qt::AlignVCenter, folderHeader);

        // Add folder button
        if (m_foldersExpanded) {
            QRect addRect(width() - 26, sectionY + 4, 18, 18);
            bool isAddHovered = m_hoveredAddButton;
            if (isAddHovered)
                p.fillRect(addRect, QColor(0xff, 0xff, 0xff, 15));
            p.setPen(Color::TEXT_SECONDARY);
            p.drawText(addRect, Qt::AlignCenter, "+");
        }

        // Bottom separator
        p.setPen(Color::BORDER);
        p.drawLine(12, sectionRect.bottom(), width() - 12, sectionRect.bottom());

        if (m_foldersExpanded) {
            QFont f = p.font();
            f.setBold(false);
            p.setFont(f);
            for (int i = 0; i < m_folders.size(); i++) {
                QRect itemRect(0, kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
                bool isHovered = (i == m_hoveredFolder);
                bool isActive = (m_folders[i] == m_activeFolder);
                bool isFocused = (m_focusSection == FocusFolders && m_focusIndex == i);

                if (isActive) {
                    p.fillRect(itemRect, Color::BG_SELECTED);
                    p.fillRect(itemRect.left(), itemRect.top(), 2, itemRect.height(), Color::ACCENT);
                } else if (isFocused) {
                    p.fillRect(itemRect, Color::BG_MEDIUM);
                } else if (isHovered) {
                    p.fillRect(itemRect, Color::BG_HOVER);
                }
                if (isFocused)
                    drawFocusRect(p, itemRect);

                bool folderActive = (m_folders[i] == m_activeFolder);
                bool folderHover = (i == m_hoveredFolder);
                drawFolderIcon(p, itemRect, folderHover, folderActive);

                p.setPen(Color::TEXT_PRIMARY);
                p.drawText(itemRect.adjusted(22, 0, 0, 0), Qt::AlignVCenter,
                           p.fontMetrics().elidedText(m_folders[i], Qt::ElideLeft, width() - 30));
            }
        }
    }

    // -- Tags section --
    int tagY = m_sectionTagY;
    QRect tagSectionRect(0, tagY, width(), kSectionHeight);
    p.setPen(QColor(0x96, 0x96, 0x96));
    QFont sf2 = p.font();
    sf2.setBold(false);
    p.setFont(sf2);
    QString tagHeader = Codicon::icon(m_tagsExpanded ? "chevron-down" : "chevron-right")
                      + "  " + QStringLiteral("标签");
    p.drawText(tagSectionRect.adjusted(12, 0, 0, 0), Qt::AlignVCenter, tagHeader);

    // Bottom separator
        p.setPen(Color::BORDER);
    p.drawLine(12, tagSectionRect.bottom(), width() - 12, tagSectionRect.bottom());

    if (m_tagsExpanded) {
        QFont f2 = p.font();
        f2.setBold(false);
        p.setFont(f2);
        for (int i = 0; i < m_tags.size(); i++) {
            QRect itemRect(0, tagY + kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
            bool isHovered = (i == m_hoveredTag);
            bool isActive = (m_tags[i].id == m_activeTagId);
            bool isFocused = (m_focusSection == FocusTags && m_focusIndex == i);

            if (isActive) {
                p.fillRect(itemRect, Color::BG_SELECTED);
                p.fillRect(itemRect.left(), itemRect.top(), 2, itemRect.height(), Color::ACCENT);
            } else if (isFocused) {
                p.fillRect(itemRect, Color::BG_MEDIUM);
            } else if (isHovered) {
                p.fillRect(itemRect, Color::BG_HOVER);
            }
            if (isFocused)
                drawFocusRect(p, itemRect);

            drawTagDot(p, QPoint(14, itemRect.center().y()), m_tags[i].color);

            p.setPen(QColor(0xcc, 0xcc, 0xcc));
            p.drawText(itemRect.adjusted(22, 0, 0, 0), Qt::AlignVCenter,
                       p.fontMetrics().elidedText(m_tags[i].name, Qt::ElideRight, width() - 30));
        }
    }
}

void SidebarWidget::mouseMoveEvent(QMouseEvent *event)
{
    int oldHoverFolder = m_hoveredFolder;
    int oldHoverTag = m_hoveredTag;
    bool oldHoverAdd = m_hoveredAddButton;
    m_hoveredFolder = -1;
    m_hoveredTag = -1;
    m_hoveredAddButton = false;

    // Check add button
    QRect addRect(width() - 26, 6, 20, 20);
    if (addRect.contains(event->pos()))
        m_hoveredAddButton = true;

    for (int i = 0; i < m_folders.size(); i++) {
        QRect r(0, kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
        if (r.contains(event->pos())) {
            m_hoveredFolder = i;
            break;
        }
    }
    for (int i = 0; i < m_tags.size(); i++) {
        QRect r(0, m_sectionTagY + kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
        if (r.contains(event->pos())) {
            m_hoveredTag = i;
            break;
        }
    }

    if (oldHoverFolder != m_hoveredFolder || oldHoverTag != m_hoveredTag || oldHoverAdd != m_hoveredAddButton)
        update();
}

void SidebarWidget::mousePressEvent(QMouseEvent *event)
{
    // Folders section toggle
    if (!m_folders.isEmpty()) {
        QRect folderSectionHeader(0, 0, width(), kSectionHeight);
        if (folderSectionHeader.contains(event->pos())) {
            setFocus();
            m_focusSection = FocusFolders;
            m_focusIndex = 0;
            m_foldersExpanded = !m_foldersExpanded;
            updateLayout();
            update();
            return;
        }
    }
    // Tags section toggle
    if (!m_tags.isEmpty()) {
        QRect tagSectionHeader(0, m_sectionTagY, width(), kSectionHeight);
        if (tagSectionHeader.contains(event->pos())) {
            setFocus();
            m_focusSection = FocusTags;
            m_focusIndex = 0;
            m_tagsExpanded = !m_tagsExpanded;
            update();
            return;
        }
    }

    // Add button
    QRect addRect(width() - 26, 6, 20, 20);
    if (addRect.contains(event->pos())) {
        emit addFolderClicked();
        return;
    }

    for (int i = 0; i < m_folders.size(); i++) {
        QRect r(0, kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
        if (r.contains(event->pos())) {
            setFocus();
            m_focusSection = FocusFolders;
            m_focusIndex = i;
            setActiveFolder(m_folders[i]);
            emit folderSelected(m_folders[i]);
            return;
        }
    }
    for (int i = 0; i < m_tags.size(); i++) {
        QRect r(0, m_sectionTagY + kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
        if (r.contains(event->pos())) {
            setFocus();
            m_focusSection = FocusTags;
            m_focusIndex = i;
            setActiveTag(m_tags[i].id);
            emit tagSelected(m_tags[i].id);
            return;
        }
    }
}

void SidebarWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    for (int i = 0; i < m_folders.size(); i++) {
        QRect r(0, kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
        if (r.contains(event->pos())) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_folders[i]));
            return;
        }
    }
}

void SidebarWidget::keyPressEvent(QKeyEvent *event)
{
    int folderCount = m_foldersExpanded ? m_folders.size() : 0;
    int tagCount = m_tagsExpanded ? m_tags.size() : 0;

    if (event->key() == Qt::Key_Down) {
        if (m_focusSection == FocusFolders && m_focusIndex < folderCount - 1) {
            m_focusIndex++;
        } else if (m_focusSection == FocusFolders && folderCount > 0) {
            // Move to tags if available
            if (tagCount > 0) { m_focusSection = FocusTags; m_focusIndex = 0; }
        } else if (m_focusSection == FocusTags && m_focusIndex < tagCount - 1) {
            m_focusIndex++;
        } else if (m_focusSection == FocusNone) {
            if (folderCount > 0) { m_focusSection = FocusFolders; m_focusIndex = 0; }
            else if (tagCount > 0) { m_focusSection = FocusTags; m_focusIndex = 0; }
        }
        update();
    } else if (event->key() == Qt::Key_Up) {
        if (m_focusSection == FocusTags && m_focusIndex > 0) {
            m_focusIndex--;
        } else if (m_focusSection == FocusTags && tagCount > 0) {
            if (folderCount > 0) { m_focusSection = FocusFolders; m_focusIndex = folderCount - 1; }
        } else if (m_focusSection == FocusFolders && m_focusIndex > 0) {
            m_focusIndex--;
        } else if (m_focusSection == FocusNone) {
            if (tagCount > 0) { m_focusSection = FocusTags; m_focusIndex = tagCount - 1; }
            else if (folderCount > 0) { m_focusSection = FocusFolders; m_focusIndex = folderCount - 1; }
        }
        update();
    } else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Return || event->key() == Qt::Key_Space) {
        if (event->key() == Qt::Key_Right && m_focusSection == FocusFolders && !m_foldersExpanded) {
            m_foldersExpanded = true;
            updateLayout(); update();
        } else if (event->key() == Qt::Key_Right && m_focusSection == FocusTags && !m_tagsExpanded) {
            m_tagsExpanded = true;
            updateLayout(); update();
        } else {
            activateCurrentFocus();
        }
    } else if (event->key() == Qt::Key_Left) {
        if (m_focusSection == FocusFolders && m_foldersExpanded) {
            m_foldersExpanded = false;
            updateLayout(); update();
        } else if (m_focusSection == FocusTags && m_tagsExpanded) {
            m_tagsExpanded = false;
            updateLayout(); update();
        }
    } else if (event->key() == Qt::Key_Home) {
        if (folderCount > 0) { m_focusSection = FocusFolders; m_focusIndex = 0; }
        else if (tagCount > 0) { m_focusSection = FocusTags; m_focusIndex = 0; }
        update();
    } else if (event->key() == Qt::Key_End) {
        if (tagCount > 0) { m_focusSection = FocusTags; m_focusIndex = tagCount - 1; }
        else if (folderCount > 0) { m_focusSection = FocusFolders; m_focusIndex = folderCount - 1; }
        update();
    } else if (event->key() == Qt::Key_Menu) {
        showContextMenuForCurrentFocus();
    } else if (event->key() == Qt::Key_F2) {
        if (m_focusSection == FocusTags && m_focusIndex >= 0 && m_focusIndex < m_tags.size())
            emit tagEditRequested(m_tags[m_focusIndex].id);
    } else if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (m_focusSection == FocusFolders && m_focusIndex >= 0 && m_focusIndex < m_folders.size())
            emit folderRemoveRequested(m_folders[m_focusIndex]);
        else if (m_focusSection == FocusTags && m_focusIndex >= 0 && m_focusIndex < m_tags.size())
            emit tagDeleteRequested(m_tags[m_focusIndex].id);
    } else {
        QWidget::keyPressEvent(event);
    }
}

void SidebarWidget::drawFocusRect(QPainter &p, const QRect &r) const
{
    p.setPen(QPen(Color::ACCENT, 1, Qt::DotLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(1, 1, -2, -1));
}

void SidebarWidget::activateCurrentFocus()
{
    if (m_focusSection == FocusFolders && m_focusIndex >= 0 && m_focusIndex < m_folders.size()) {
        setActiveFolder(m_folders[m_focusIndex]);
        emit folderSelected(m_folders[m_focusIndex]);
    } else if (m_focusSection == FocusTags && m_focusIndex >= 0 && m_focusIndex < m_tags.size()) {
        setActiveTag(m_tags[m_focusIndex].id);
        emit tagSelected(m_tags[m_focusIndex].id);
    }
}

void SidebarWidget::showFolderContextMenu(int idx, const QPoint &pos)
{
    QMenu menu(this);
    QPalette mPal;
    mPal.setColor(QPalette::Window, Color::BG_DARK);
    mPal.setColor(QPalette::WindowText, Color::TEXT_PRIMARY);
    mPal.setColor(QPalette::Highlight, Color::HIGHLIGHT);
    mPal.setColor(QPalette::HighlightedText, Color::TEXT_PRIMARY);
    menu.setPalette(mPal);

    QAction *refreshAction = menu.addAction(QString::fromUtf8("\xe5\x88\xb7\xe6\x96\xb0"));
    { QPixmap px(16, 16); px.fill(Qt::transparent); QPainter sp(&px); Codicon::draw(sp, "search", QRect(0, 0, 16, 16), Color::TEXT_PRIMARY, 14); refreshAction->setIcon(QIcon(px)); }
    connect(refreshAction, &QAction::triggered, this, [this, idx]() {
        emit folderRefreshRequested(m_folders[idx]);
    });

    QAction *removeAction = menu.addAction(QString::fromUtf8("\xe4\xbb\x8e\xe5\xba\x93\xe4\xb8\xad\xe7\xa7\xbb\xe9\x99\xa4"));
    { QPixmap px(16, 16); px.fill(Qt::transparent); QPainter sp(&px); Codicon::draw(sp, "trash", QRect(0, 0, 16, 16), Color::TEXT_PRIMARY, 14); removeAction->setIcon(QIcon(px)); }
    connect(removeAction, &QAction::triggered, this, [this, idx]() {
        emit folderRemoveRequested(m_folders[idx]);
    });

    menu.addSeparator();

    QAction *explorerAction = menu.addAction(QString::fromUtf8("\xe5\x9c\xa8\xe8\xb5\x84\xe6\xba\x90\xe7\xae\xa1\xe7\x90\x86\xe5\x99\xa8\xe4\xb8\xad\xe6\x89\x93\xe5\xbc\x80"));
    { QPixmap px(16, 16); px.fill(Qt::transparent); QPainter sp(&px); Codicon::draw(sp, "folder", QRect(0, 0, 16, 16), Color::TEXT_PRIMARY, 14); explorerAction->setIcon(QIcon(px)); }
    connect(explorerAction, &QAction::triggered, this, [this, idx]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_folders[idx]));
    });

    menu.exec(pos);
}

void SidebarWidget::showTagContextMenu(int idx, const QPoint &pos)
{
    QMenu menu(this);
    QPalette mPal;
    mPal.setColor(QPalette::Window, Color::BG_DARK);
    mPal.setColor(QPalette::WindowText, Color::TEXT_PRIMARY);
    mPal.setColor(QPalette::Highlight, Color::HIGHLIGHT);
    mPal.setColor(QPalette::HighlightedText, Color::TEXT_PRIMARY);
    menu.setPalette(mPal);

    QAction *editAction = menu.addAction(QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91\xe6\xa0\x87\xe7\xad\xbe"));
    { QPixmap px(16, 16); px.fill(Qt::transparent); QPainter sp(&px); Codicon::draw(sp, "edit", QRect(0, 0, 16, 16), Color::TEXT_PRIMARY, 14); editAction->setIcon(QIcon(px)); }
    connect(editAction, &QAction::triggered, this, [this, idx]() {
        emit tagEditRequested(m_tags[idx].id);
    });

    QAction *delAction = menu.addAction(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4\xe6\xa0\x87\xe7\xad\xbe"));
    { QPixmap px(16, 16); px.fill(Qt::transparent); QPainter sp(&px); Codicon::draw(sp, "trash", QRect(0, 0, 16, 16), Color::TEXT_PRIMARY, 14); delAction->setIcon(QIcon(px)); }
    connect(delAction, &QAction::triggered, this, [this, idx]() {
        emit tagDeleteRequested(m_tags[idx].id);
    });

    menu.exec(pos);
}

void SidebarWidget::showContextMenuForCurrentFocus()
{
    if (m_focusSection == FocusFolders && m_focusIndex >= 0 && m_focusIndex < m_folders.size()) {
        QPoint pos = mapToGlobal(QPoint(width() / 2, kSectionHeight + 1 + m_focusIndex * kItemHeight));
        showFolderContextMenu(m_focusIndex, pos);
    } else if (m_focusSection == FocusTags && m_focusIndex >= 0 && m_focusIndex < m_tags.size()) {
        QPoint pos = mapToGlobal(QPoint(width() / 2, m_sectionTagY + kSectionHeight + 1 + m_focusIndex * kItemHeight));
        showTagContextMenu(m_focusIndex, pos);
    }
}

void SidebarWidget::contextMenuEvent(QContextMenuEvent *event)
{
    for (int i = 0; i < m_folders.size(); i++) {
        QRect r(0, kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
        if (r.contains(event->pos())) {
            showFolderContextMenu(i, event->globalPos());
            return;
        }
    }

    for (int i = 0; i < m_tags.size(); i++) {
        QRect r(0, m_sectionTagY + kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
        if (r.contains(event->pos())) {
            showTagContextMenu(i, event->globalPos());
            return;
        }
    }
}

void SidebarWidget::resizeEvent(QResizeEvent *)
{
    updateLayout();
    update();
}
