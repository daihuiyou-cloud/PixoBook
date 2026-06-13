#include "SidebarWidget.h"
#include <QAction>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QUrl>
#include "ui/Codicon.h"
#include "ui/ColorConstants.h"
#include "ui/VisualConstants.h"
#include "ui/UIUtils.h"

SidebarWidget::SidebarWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFixedWidth(Visual::SidebarWidth);
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
    int folderRows = m_foldersExpanded ? qMax(1, m_folders.size()) * kItemHeight : 0;
    m_sectionTagY = kSectionHeight + folderRows + 8;
}

void SidebarWidget::drawFolderIcon(QPainter &p, const QRect &r, bool hovered, bool active) const
{
    QColor fc = active ? Color::TEXT_BRIGHT : (hovered ? Color::TEXT_PRIMARY : Color::TEXT_SECONDARY);
    Codicon::draw(p, "folder", QRect(r.x() + 12, r.y(), 18, r.height()), fc, 16);
}

void SidebarWidget::drawTagDot(QPainter &p, const QPoint &center, const QColor &color) const
{
    Codicon::draw(p, "tag", QRect(center.x() - 8, center.y() - 8, 16, 16), color, 16);
}

void SidebarWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Color::BG_DARK);

    QFont sectionFont = font();
    sectionFont.setPixelSize(Visual::FontMeta);
    sectionFont.setBold(true);
    QFont itemFont = font();
    itemFont.setPixelSize(Visual::FontControl);

    QRect folderHeader(0, 0, width(), kSectionHeight);
    p.setFont(sectionFont);
    p.setPen(Color::TEXT_SECONDARY);
    Codicon::draw(p, m_foldersExpanded ? "chevron-down" : "chevron-right",
                  QRect(8, 0, 14, kSectionHeight), Color::TEXT_SECONDARY, 14);
    p.setFont(sectionFont);
    p.drawText(folderHeader.adjusted(28, 0, 0, 0), Qt::AlignVCenter, tr("素材文件夹"));

    QRect addRect(width() - 30, 5, 22, 22);
    if (m_hoveredAddButton)
        p.fillRect(addRect.adjusted(1, 1, -1, -1), Color::BG_BUTTON_HOVER);
    Codicon::draw(p, "add", addRect, Color::TEXT_SECONDARY, 13);
    p.setPen(Color::BORDER_MUTED);
    p.drawLine(12, folderHeader.bottom(), width() - 12, folderHeader.bottom());

    if (m_foldersExpanded) {
        p.setFont(itemFont);
        if (m_folders.isEmpty()) {
            p.setPen(Color::TEXT_SECONDARY);
            p.drawText(QRect(16, kSectionHeight + 4, width() - 32, kItemHeight),
                       Qt::AlignVCenter | Qt::AlignLeft, tr("添加素材文件夹"));
        }
        for (int i = 0; i < m_folders.size(); i++) {
            QRect itemRect(0, kSectionHeight + i * kItemHeight, width(), kItemHeight);
            bool isHovered = i == m_hoveredFolder;
            bool isActive = m_folders[i] == m_activeFolder;
            bool isFocused = m_focusSection == FocusFolders && m_focusIndex == i;

            if (isActive) {
                p.fillRect(itemRect.adjusted(4, 2, -4, -2), Color::BG_SELECTED);
                p.fillRect(0, itemRect.top() + 4, 3, itemRect.height() - 8, Color::ACCENT);
            } else if (isFocused || isHovered) {
                p.fillRect(itemRect.adjusted(4, 2, -4, -2), isFocused ? Color::BG_MEDIUM : Color::BG_HOVER);
            }
            if (isFocused) drawFocusRect(p, itemRect.adjusted(4, 2, -4, -2));

            drawFolderIcon(p, itemRect, isHovered, isActive);
            QString display = QFileInfo(m_folders[i]).fileName();
            if (display.isEmpty()) display = m_folders[i];
            p.setPen(isActive ? Color::TEXT_BRIGHT : Color::TEXT_PRIMARY);
            p.setFont(itemFont);
            p.drawText(itemRect.adjusted(36, 0, -10, 0), Qt::AlignVCenter,
                       p.fontMetrics().elidedText(display, Qt::ElideRight, width() - 50));
        }
    }

    QRect tagHeader(0, m_sectionTagY, width(), kSectionHeight);
    p.setFont(sectionFont);
    p.setPen(Color::TEXT_SECONDARY);
    Codicon::draw(p, m_tagsExpanded ? "chevron-down" : "chevron-right",
                  QRect(8, tagHeader.top(), 14, kSectionHeight), Color::TEXT_SECONDARY, 14);
    p.setFont(sectionFont);
    p.drawText(tagHeader.adjusted(28, 0, 0, 0), Qt::AlignVCenter, tr("标签"));
    p.setPen(Color::BORDER_MUTED);
    p.drawLine(12, tagHeader.bottom(), width() - 12, tagHeader.bottom());

    if (m_tagsExpanded) {
        p.setFont(itemFont);
        if (m_tags.isEmpty()) {
            p.setPen(Color::TEXT_SECONDARY);
            p.drawText(QRect(16, m_sectionTagY + kSectionHeight + 4, width() - 32, kItemHeight),
                       Qt::AlignVCenter | Qt::AlignLeft, tr("暂无标签"));
            m_addTagRect = QRect(16, m_sectionTagY + kSectionHeight + kItemHeight + 4, width() - 32, 26);
            p.setBrush(m_hoveredAddTagButton ? Color::BG_BUTTON_HOVER : Color::BG_BUTTON);
            p.setPen(QPen(m_hoveredAddTagButton ? Color::TEXT_PRIMARY : Color::BORDER_SUBTLE, 1));
            p.drawRoundedRect(m_addTagRect, Visual::RadiusSmall, Visual::RadiusSmall);
            QColor addColor = m_hoveredAddTagButton ? Color::TEXT_PRIMARY : Color::TEXT_SECONDARY;
            Codicon::draw(p, "add", QRect(m_addTagRect.left() + 8, m_addTagRect.top(), 14, m_addTagRect.height()),
                          addColor, 12);
            p.setPen(addColor);
            p.drawText(m_addTagRect.adjusted(28, 0, -8, 0),
                       Qt::AlignVCenter | Qt::AlignLeft, tr("添加标签"));
        } else {
            m_addTagRect = {};
        }
        for (int i = 0; i < m_tags.size(); i++) {
            QRect itemRect(0, m_sectionTagY + kSectionHeight + i * kItemHeight, width(), kItemHeight);
            bool isHovered = i == m_hoveredTag;
            bool isActive = m_tags[i].id == m_activeTagId;
            bool isFocused = m_focusSection == FocusTags && m_focusIndex == i;

            if (isActive) {
                p.fillRect(itemRect.adjusted(4, 2, -4, -2), Color::BG_SELECTED);
                p.fillRect(0, itemRect.top() + 4, 3, itemRect.height() - 8, Color::ACCENT);
            } else if (isFocused || isHovered) {
                p.fillRect(itemRect.adjusted(4, 2, -4, -2), isFocused ? Color::BG_MEDIUM : Color::BG_HOVER);
            }
            if (isFocused) drawFocusRect(p, itemRect.adjusted(4, 2, -4, -2));

            drawTagDot(p, QPoint(22, itemRect.center().y()), m_tags[i].color);
            p.setPen(isActive ? Color::TEXT_BRIGHT : Color::TEXT_PRIMARY);
            p.setFont(itemFont);
            p.drawText(itemRect.adjusted(38, 0, -10, 0), Qt::AlignVCenter,
                       p.fontMetrics().elidedText(m_tags[i].name, Qt::ElideRight, width() - 52));
        }
    }
}

void SidebarWidget::mouseMoveEvent(QMouseEvent *event)
{
    int oldHoverFolder = m_hoveredFolder;
    int oldHoverTag = m_hoveredTag;
    bool oldHoverAdd = m_hoveredAddButton;
    bool oldHoverAddTag = m_hoveredAddTagButton;
    m_hoveredFolder = -1;
    m_hoveredTag = -1;
    m_hoveredAddButton = false;
    m_hoveredAddTagButton = false;

    QRect addRect(width() - 30, 5, 22, 22);
    if (addRect.contains(event->pos()))
        m_hoveredAddButton = true;
    if (m_addTagRect.contains(event->pos()))
        m_hoveredAddTagButton = true;

    if (m_foldersExpanded) {
        for (int i = 0; i < m_folders.size(); i++) {
            QRect r(0, kSectionHeight + i * kItemHeight, width(), kItemHeight);
            if (r.contains(event->pos())) {
                m_hoveredFolder = i;
                break;
            }
        }
    }
    if (m_tagsExpanded) {
        for (int i = 0; i < m_tags.size(); i++) {
            QRect r(0, m_sectionTagY + kSectionHeight + i * kItemHeight, width(), kItemHeight);
            if (r.contains(event->pos())) {
                m_hoveredTag = i;
                break;
            }
        }
    }

    if (oldHoverFolder != m_hoveredFolder) {
        if (m_hoveredFolder >= 0)
            QToolTip::showText(event->globalPos(), m_folders[m_hoveredFolder], this);
        else
            QToolTip::hideText();
    }

    if (oldHoverFolder != m_hoveredFolder || oldHoverTag != m_hoveredTag
        || oldHoverAdd != m_hoveredAddButton || oldHoverAddTag != m_hoveredAddTagButton)
        update();
}

void SidebarWidget::mousePressEvent(QMouseEvent *event)
{
    QRect addRect(width() - 30, 5, 22, 22);
    if (addRect.contains(event->pos())) {
        emit addFolderClicked();
        return;
    }
    if (m_addTagRect.contains(event->pos())) {
        emit tagCreateRequested();
        return;
    }

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

    QRect tagSectionHeader(0, m_sectionTagY, width(), kSectionHeight);
    if (tagSectionHeader.contains(event->pos())) {
        setFocus();
        m_focusSection = FocusTags;
        m_focusIndex = 0;
        m_tagsExpanded = !m_tagsExpanded;
        updateLayout();
        update();
        return;
    }

    if (m_foldersExpanded) {
        for (int i = 0; i < m_folders.size(); i++) {
            QRect r(0, kSectionHeight + i * kItemHeight, width(), kItemHeight);
            if (r.contains(event->pos())) {
                setFocus();
                m_focusSection = FocusFolders;
                m_focusIndex = i;
                setActiveFolder(m_folders[i]);
                emit folderSelected(m_folders[i]);
                return;
            }
        }
    }
    if (m_tagsExpanded) {
        for (int i = 0; i < m_tags.size(); i++) {
            QRect r(0, m_sectionTagY + kSectionHeight + i * kItemHeight, width(), kItemHeight);
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
}

void SidebarWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_foldersExpanded) return;
    for (int i = 0; i < m_folders.size(); i++) {
        QRect r(0, kSectionHeight + i * kItemHeight, width(), kItemHeight);
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
        if (m_focusSection == FocusFolders && m_focusIndex < folderCount - 1) m_focusIndex++;
        else if (m_focusSection == FocusFolders && tagCount > 0) { m_focusSection = FocusTags; m_focusIndex = 0; }
        else if (m_focusSection == FocusTags && m_focusIndex < tagCount - 1) m_focusIndex++;
        else if (m_focusSection == FocusNone) {
            if (folderCount > 0) { m_focusSection = FocusFolders; m_focusIndex = 0; }
            else if (tagCount > 0) { m_focusSection = FocusTags; m_focusIndex = 0; }
        }
        update();
    } else if (event->key() == Qt::Key_Up) {
        if (m_focusSection == FocusTags && m_focusIndex > 0) m_focusIndex--;
        else if (m_focusSection == FocusTags && folderCount > 0) { m_focusSection = FocusFolders; m_focusIndex = folderCount - 1; }
        else if (m_focusSection == FocusFolders && m_focusIndex > 0) m_focusIndex--;
        else if (m_focusSection == FocusNone) {
            if (tagCount > 0) { m_focusSection = FocusTags; m_focusIndex = tagCount - 1; }
            else if (folderCount > 0) { m_focusSection = FocusFolders; m_focusIndex = folderCount - 1; }
        }
        update();
    } else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Return || event->key() == Qt::Key_Space) {
        if (event->key() == Qt::Key_Right && m_focusSection == FocusFolders && !m_foldersExpanded) {
            m_foldersExpanded = true; updateLayout(); update();
        } else if (event->key() == Qt::Key_Right && m_focusSection == FocusTags && !m_tagsExpanded) {
            m_tagsExpanded = true; updateLayout(); update();
        } else {
            activateCurrentFocus();
        }
    } else if (event->key() == Qt::Key_Left) {
        if (m_focusSection == FocusFolders && m_foldersExpanded) {
            m_foldersExpanded = false; updateLayout(); update();
        } else if (m_focusSection == FocusTags && m_tagsExpanded) {
            m_tagsExpanded = false; updateLayout(); update();
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
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), Visual::RadiusSmall, Visual::RadiusSmall);
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
    UIUtils::setupMenuPalette(menu);

    QAction *refreshAction = menu.addAction(tr("刷新"));
    connect(refreshAction, &QAction::triggered, this, [this, idx]() { emit folderRefreshRequested(m_folders[idx]); });

    QAction *removeAction = menu.addAction(tr("从库中移除"));
    connect(removeAction, &QAction::triggered, this, [this, idx]() { emit folderRemoveRequested(m_folders[idx]); });

    menu.addSeparator();
    QAction *explorerAction = menu.addAction(tr("在资源管理器中打开"));
    connect(explorerAction, &QAction::triggered, this, [this, idx]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_folders[idx]));
    });

    menu.exec(pos);
}

void SidebarWidget::showTagContextMenu(int idx, const QPoint &pos)
{
    QMenu menu(this);
    UIUtils::setupMenuPalette(menu);

    QAction *editAction = menu.addAction(tr("编辑标签"));
    connect(editAction, &QAction::triggered, this, [this, idx]() { emit tagEditRequested(m_tags[idx].id); });

    QAction *delAction = menu.addAction(tr("删除标签"));
    connect(delAction, &QAction::triggered, this, [this, idx]() { emit tagDeleteRequested(m_tags[idx].id); });

    menu.exec(pos);
}

void SidebarWidget::showContextMenuForCurrentFocus()
{
    if (m_focusSection == FocusFolders && m_focusIndex >= 0 && m_focusIndex < m_folders.size()) {
        QPoint pos = mapToGlobal(QPoint(width() / 2, kSectionHeight + m_focusIndex * kItemHeight));
        showFolderContextMenu(m_focusIndex, pos);
    } else if (m_focusSection == FocusTags && m_focusIndex >= 0 && m_focusIndex < m_tags.size()) {
        QPoint pos = mapToGlobal(QPoint(width() / 2, m_sectionTagY + kSectionHeight + m_focusIndex * kItemHeight));
        showTagContextMenu(m_focusIndex, pos);
    }
}

void SidebarWidget::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_foldersExpanded) {
        for (int i = 0; i < m_folders.size(); i++) {
            QRect r(0, kSectionHeight + i * kItemHeight, width(), kItemHeight);
            if (r.contains(event->pos())) {
                showFolderContextMenu(i, event->globalPos());
                return;
            }
        }
    }
    if (m_tagsExpanded) {
        for (int i = 0; i < m_tags.size(); i++) {
            QRect r(0, m_sectionTagY + kSectionHeight + i * kItemHeight, width(), kItemHeight);
            if (r.contains(event->pos())) {
                showTagContextMenu(i, event->globalPos());
                return;
            }
        }
    }
}

void SidebarWidget::resizeEvent(QResizeEvent *)
{
    updateLayout();
    update();
}
