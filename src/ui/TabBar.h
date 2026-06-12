#ifndef TABBAR_H
#define TABBAR_H

#include <QWidget>
#include <QVector>
#include <QString>

class TabBar : public QWidget
{
    Q_OBJECT
public:
    struct Tab {
        QString label;
        QString icon;
    };

    explicit TabBar(QWidget *parent = nullptr);

    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int idx);
    void setTabs(const QVector<Tab> &tabs);

signals:
    void currentChanged(int index);
    void addTabRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRect tabRect(int index) const;
    QRect addButtonRect() const;
    int indexAt(const QPoint &pos) const;

    QVector<Tab> m_tabs;
    int m_currentIndex = 0;
    int m_hoveredIndex = -1;
    bool m_hoveredAdd = false;

    static constexpr int kTabHeight = 38;
    static constexpr int kTabMinWidth = 96;
    static constexpr int kTabPadding = 16;
};

#endif
