#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include <QWidget>
#include <QVector>
#include <QStringList>
#include "models/Tag.h"

class SidebarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SidebarWidget(QWidget *parent = nullptr);

    void setTags(const QVector<Tag> &tags);
    void setFolders(const QStringList &folders);
    void setActiveFolder(const QString &folder);
    void setActiveTag(int tagId);

signals:
    void folderSelected(const QString &path);
    void tagSelected(int tagId);
    void addFolderClicked();
    void folderRefreshRequested(const QString &path);
    void folderRemoveRequested(const QString &path);
    void folderOpenInExplorer(const QString &path);
    void tagEditRequested(int tagId);
    void tagDeleteRequested(int tagId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void drawFolderIcon(QPainter &p, const QRect &r, bool hovered, bool active) const;
    void drawTagDot(QPainter &p, const QPoint &center, const QColor &color) const;

    QVector<Tag> m_tags;
    QStringList m_folders;
    QString m_activeFolder;
    int m_activeTagId = -1;
    bool m_foldersExpanded = true;
    bool m_tagsExpanded = true;

    int m_sectionTagY = 0;
    static constexpr int kItemHeight = 22;
    static constexpr int kSectionHeight = 22;

    int m_hoveredFolder = -1;
    int m_hoveredTag = -1;
    bool m_hoveredAddButton = false;

    void updateLayout();
};

#endif
