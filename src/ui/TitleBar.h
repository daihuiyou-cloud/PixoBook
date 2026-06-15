#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <QString>

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setMaximized(bool maximized);
    bool isMaximized() const { return m_maximized; }

    QRect menuItemRect(int idx) const;

signals:
    void menuTriggered(int menuIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    enum ControlButton { CtrlNone = -1, CtrlMinimize, CtrlMaximize, CtrlClose };
    enum HitTest {
        HitNone,
        HitMenu,
        HitMinimize,
        HitMaximize,
        HitClose,
        HitDrag
    };

    HitTest hitTest(const QPoint &pos) const;
    QRect closeBtnRect() const;
    QRect maximizeBtnRect() const;
    QRect minimizeBtnRect() const;

    bool m_maximized = false;
    int m_hoveredMenu = -1;
    ControlButton m_hoveredControl = CtrlNone;
    ControlButton m_pressedControl = CtrlNone;
    bool m_dragging = false;
    QPoint m_dragStartPos;

    struct MenuDef {
        QString text;
        QString iconName;
    };
    QVector<MenuDef> m_menus;
    QFont m_titleFont;
    static constexpr int kHeight = 32;
};

#endif
