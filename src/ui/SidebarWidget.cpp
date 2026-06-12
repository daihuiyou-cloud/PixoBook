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
    QColor fc = active ? QColor(0xcc, 0xcc, 0xcc) : (hovered ? QColor(0xc0, 0xa0, 0x60) : QColor(0x96, 0x96, 0x96));
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

    p.fillRect(rect(), QColor(0x25, 0x25, 0x26));

    // -- Folders section --
    if (!m_folders.isEmpty()) {
        int sectionY = 0;
        QRect sectionRect(0, sectionY, width(), kSectionHeight);
        p.setPen(QColor(0x96, 0x96, 0x96));
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
            p.setPen(QColor(0x96, 0x96, 0x96));
            p.drawText(addRect, Qt::AlignCenter, "+");
        }

        // Bottom separator
        p.setPen(QColor(0x3c, 0x3c, 0x3c));
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
                    p.fillRect(itemRect, QColor(0x37, 0x37, 0x3d));
                    p.fillRect(itemRect.left(), itemRect.top(), 2, itemRect.height(), QColor(0x00, 0x7a, 0xcc));
                } else if (isFocused) {
                    p.fillRect(itemRect, QColor(0x2d, 0x2d, 0x30));
                } else if (isHovered) {
                    p.fillRect(itemRect, QColor(0x2a, 0x2d, 0x2e));
                }
                if (isFocused)
                    drawFocusRect(p, itemRect);

                bool folderActive = (m_folders[i] == m_activeFolder);
                bool folderHover = (i == m_hoveredFolder);
                drawFolderIcon(p, itemRect, folderHover, folderActive);

                p.setPen(QColor(0xcc, 0xcc, 0xcc));
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
    p.setPen(QColor(0x3c, 0x3c, 0x3c));
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
                p.fillRect(itemRect, QColor(0x37, 0x37, 0x3d));
                p.fillRect(itemRect.left(), itemRect.top(), 2, itemRect.height(), QColor(0x00, 0x7a, 0xcc));
            } else if (isFocused) {
                p.fillRect(itemRect, QColor(0x2d, 0x2d, 0x30));
            } else if (isHovered) {
                p.fillRect(itemRect, QColor(0x2a, 0x2d, 0x2e));
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
    p.setPen(QPen(QColor(0x00, 0x7a, 0xcc), 1, Qt::DotLine));
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

void SidebarWidget::showContextMenuForCurrentFocus()
{
    QMenu menu(this);
    QPalette mPal;
    mPal.setColor(QPalette::Window, QColor(0x25, 0x25, 0x26));
    mPal.setColor(QPalette::WindowText, QColor(0xcc, 0xcc, 0xcc));
    mPal.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
    mPal.setColor(QPalette::HighlightedText, QColor(0xcc, 0xcc, 0xcc));
    menu.setPalette(mPal);

    if (m_focusSection == FocusFolders && m_focusIndex >= 0 && m_focusIndex < m_folders.size()) {
        QAction *refreshAction = menu.addAction(QString::fromUtf8("\xe5\x88\xb7\xe6\x96\xb0"));
        connect(refreshAction, &QAction::triggered, this, [this]() {
            emit folderRefreshRequested(m_folders[m_focusIndex]);
        });
        QAction *removeAction = menu.addAction(QString::fromUtf8("\xe4\xbb\x8e\xe5\xba\x93\xe4\xb8\xad\xe7\xa7\xbb\xe9\x99\xa4"));
        connect(removeAction, &QAction::triggered, this, [this]() {
            emit folderRemoveRequested(m_folders[m_focusIndex]);
        });
        menu.addSeparator();
        QAction *explorerAction = menu.addAction(QString::fromUtf8("\xe5\x9c\xa8\xe8\xb5\x84\xe6\xba\x90\xe7\xae\xa1\xe7\x90\x86\xe5\x99\xa8\xe4\xb8\xad\xe6\x89\x93\xe5\xbc\x80"));
        connect(explorerAction, &QAction::triggered, this, [this]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_folders[m_focusIndex]));
        });
        QPoint pos = mapToGlobal(QPoint(width() / 2, kSectionHeight + 1 + m_focusIndex * kItemHeight));
        menu.exec(pos);
    } else if (m_focusSection == FocusTags && m_focusIndex >= 0 && m_focusIndex < m_tags.size()) {
        QAction *editAction = menu.addAction(QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91\xe6\xa0\x87\xe7\xad\xbe"));
        connect(editAction, &QAction::triggered, this, [this]() {
            emit tagEditRequested(m_tags[m_focusIndex].id);
        });
        QAction *delAction = menu.addAction(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4\xe6\xa0\x87\xe7\xad\xbe"));
        connect(delAction, &QAction::triggered, this, [this]() {
            emit tagDeleteRequested(m_tags[m_focusIndex].id);
        });
        QPoint pos = mapToGlobal(QPoint(width() / 2, m_sectionTagY + kSectionHeight + 1 + m_focusIndex * kItemHeight));
        menu.exec(pos);
    }
}

void SidebarWidget::resizeEvent(QResizeEvent *)
{
    updateLayout();
    update();
}

void SidebarWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QPalette mPal;
    mPal.setColor(QPalette::Window, QColor(0x25, 0x25, 0x26));
    mPal.setColor(QPalette::WindowText, QColor(0xcc, 0xcc, 0xcc));
    mPal.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
    mPal.setColor(QPalette::HighlightedText, QColor(0xcc, 0xcc, 0xcc));
    menu.setPalette(mPal);

    // Check if clicking on a folder
    for (int i = 0; i < m_folders.size(); i++) {
        QRect r(0, kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
        if (r.contains(event->pos())) {
            QAction *refreshAction = menu.addAction(QString::fromUtf8("\xe5\x88\xb7\xe6\x96\xb0"));
            connect(refreshAction, &QAction::triggered, this, [this, i]() {
                emit folderRefreshRequested(m_folders[i]);
            });
            QAction *removeAction = menu.addAction(QString::fromUtf8("\xe4\xbb\x8e\xe5\xba\x93\xe4\xb8\xad\xe7\xa7\xbb\xe9\x99\xa4"));
            connect(removeAction, &QAction::triggered, this, [this, i]() {
                emit folderRemoveRequested(m_folders[i]);
            });
            menu.addSeparator();
            QAction *explorerAction = menu.addAction(QString::fromUtf8("\xe5\x9c\xa8\xe8\xb5\x84\xe6\xba\x90\xe7\xae\xa1\xe7\x90\x86\xe5\x99\xa8\xe4\xb8\xad\xe6\x89\x93\xe5\xbc\x80"));
            connect(explorerAction, &QAction::triggered, this, [this, i]() {
                QDesktopServices::openUrl(QUrl::fromLocalFile(m_folders[i]));
            });
            menu.exec(event->globalPos());
            return;
        }
    }

    // Check if clicking on a tag
    for (int i = 0; i < m_tags.size(); i++) {
        QRect r(0, m_sectionTagY + kSectionHeight + 1 + i * kItemHeight, width(), kItemHeight);
        if (r.contains(event->pos())) {
            QAction *editAction = menu.addAction(QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91\xe6\xa0\x87\xe7\xad\xbe"));
            connect(editAction, &QAction::triggered, this, [this, i]() {
                emit tagEditRequested(m_tags[i].id);
            });
            QAction *delAction = menu.addAction(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4\xe6\xa0\x87\xe7\xad\xbe"));
            connect(delAction, &QAction::triggered, this, [this, i]() {
                emit tagDeleteRequested(m_tags[i].id);
            });
            menu.exec(event->globalPos());
            return;
        }
    }
}
