#include "TitleBar.h"
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <QHelpEvent>
#include <QApplication>
#include "Codicon.h"
#include "ColorConstants.h"

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(kHeight);
    setMouseTracking(true);

    m_menus = {
        { QStringLiteral("\u6587\u4ef6"), "file" },
        { QStringLiteral("\u7f16\u8f91"), "edit" },
        { QStringLiteral("\u67e5\u770b"), "eye" },
        { QStringLiteral("\u5e2e\u52a9"), "question" },
    };
}

void TitleBar::setMaximized(bool maximized)
{
    m_maximized = maximized;
    update();
}

QRect TitleBar::menuItemRect(int idx) const
{
    QFont f = font();
    QFontMetrics fm(f);
    int x = 80;
    for (int i = 0; i < idx && i < m_menus.size(); i++) {
        x += fm.horizontalAdvance(m_menus[i].text) + 24;
    }
    int labelWidth = fm.horizontalAdvance(m_menus[idx].text);
    return QRect(x, 0, labelWidth + 24, kHeight);
}

QRect TitleBar::minimizeBtnRect() const
{
    return QRect(width() - 96, 0, 32, kHeight);
}

QRect TitleBar::maximizeBtnRect() const
{
    return QRect(width() - 64, 0, 32, kHeight);
}

QRect TitleBar::closeBtnRect() const
{
    return QRect(width() - 32, 0, 32, kHeight);
}

TitleBar::HitTest TitleBar::hitTest(const QPoint &pos) const
{
    if (closeBtnRect().contains(pos)) return HitClose;
    if (maximizeBtnRect().contains(pos)) return HitMaximize;
    if (minimizeBtnRect().contains(pos)) return HitMinimize;
    for (int i = 0; i < m_menus.size(); i++) {
        if (menuItemRect(i).contains(pos)) return HitMenu;
    }
    return HitDrag;
}

void TitleBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    p.fillRect(rect(), Color::BG_DARK);

    p.fillRect(QRect(0, height() - 1, width(), 1), Color::BORDER);

    Codicon::draw(p, "layout", QRect(8, 5, 24, 22), Color::TEXT_PRIMARY, 18);

    QFont f = font();
    p.setFont(f);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(QRect(32, 0, 50, kHeight), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("AI\u7d20\u6750\u5e93"));

    for (int i = 0; i < m_menus.size(); i++) {
        QRect r = menuItemRect(i);
        if (i == m_hoveredMenu) {
            p.fillRect(r, Color::BG_SELECTED);
        }
        QColor textColor = i == m_hoveredMenu ? Color::TEXT_BRIGHT : Color::TEXT_PRIMARY;
        Codicon::draw(p, m_menus[i].iconName, QRect(r.left() + 4, r.top(), 16, r.height()), textColor, 12);
        p.setPen(textColor);
        p.drawText(r.adjusted(22, 0, 0, 0), Qt::AlignVCenter, m_menus[i].text);
    }

    {
        QRect r = minimizeBtnRect();
        if (m_hoveredControl == 0) {
            p.fillRect(r, Color::BG_SELECTED);
        }
        Codicon::draw(p, "chrome-minimize", r, Color::TEXT_PRIMARY, 14);
    }

    {
        QRect r = maximizeBtnRect();
        if (m_hoveredControl == 1) {
            p.fillRect(r, Color::BG_SELECTED);
        }
        Codicon::draw(p, m_maximized ? "chrome-restore" : "chrome-maximize", r, Color::TEXT_PRIMARY, 14);
    }

    {
        QRect r = closeBtnRect();
        if (m_hoveredControl == 2) {
            p.fillRect(r, Color::CLOSE_HOVER);
            Codicon::draw(p, "chrome-close", r, Qt::white, 14);
        } else {
            Codicon::draw(p, "chrome-close", r, Color::TEXT_PRIMARY, 14);
        }
    }
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    HitTest ht = hitTest(event->pos());
    switch (ht) {
    case HitClose:
        m_pressedControl = 2;
        update();
        break;
    case HitMaximize:
        m_pressedControl = 1;
        update();
        break;
    case HitMinimize:
        m_pressedControl = 0;
        update();
        break;
    case HitMenu:
        for (int i = 0; i < m_menus.size(); i++) {
            if (menuItemRect(i).contains(event->pos())) {
                emit menuTriggered(i);
                break;
            }
        }
        break;
    case HitDrag:
        m_dragging = true;
        m_dragStartPos = event->globalPos();
        if (m_maximized) {
            QWidget *win = window();
            QPoint localPos = event->pos();
            win->showNormal();
            m_maximized = false;
            QPoint gp = event->globalPos();
            win->move(gp.x() - localPos.x(), gp.y() - localPos.y());
            m_dragStartPos = gp;
        }
        break;
    default:
        break;
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        QPoint delta = event->globalPos() - m_dragStartPos;
        QWidget *win = window();
        win->move(win->pos() + delta);
        m_dragStartPos = event->globalPos();
        return;
    }

    int oldHoverMenu = m_hoveredMenu;
    int oldHoverControl = m_hoveredControl;

    HitTest ht = hitTest(event->pos());

    m_hoveredMenu = -1;
    m_hoveredControl = -1;

    switch (ht) {
    case HitMenu:
        for (int i = 0; i < m_menus.size(); i++) {
            if (menuItemRect(i).contains(event->pos())) {
                m_hoveredMenu = i;
                break;
            }
        }
        break;
    case HitMinimize:
        m_hoveredControl = 0;
        break;
    case HitMaximize:
        m_hoveredControl = 1;
        break;
    case HitClose:
        m_hoveredControl = 2;
        break;
    default:
        break;
    }

    if (oldHoverMenu != m_hoveredMenu || oldHoverControl != m_hoveredControl) {
        update();
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (m_dragging) {
        m_dragging = false;
        update();
        return;
    }

    if (m_pressedControl == 0) {
        window()->showMinimized();
    } else if (m_pressedControl == 1) {
        QWidget *win = window();
        if (win->isMaximized()) {
            win->showNormal();
            m_maximized = false;
        } else {
            win->showMaximized();
            m_maximized = true;
        }
    } else if (m_pressedControl == 2) {
        window()->close();
    }

    m_pressedControl = -1;
    update();
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (hitTest(event->pos()) == HitDrag) {
        QWidget *win = window();
        if (win->isMaximized()) {
            win->showNormal();
            m_maximized = false;
        } else {
            win->showMaximized();
            m_maximized = true;
        }
    }
}

void TitleBar::leaveEvent(QEvent *)
{
    if (m_hoveredMenu != -1 || m_hoveredControl != -1) {
        m_hoveredMenu = -1;
        m_hoveredControl = -1;
        update();
    }
}

bool TitleBar::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(event);
        if (minimizeBtnRect().contains(he->pos()))
            QToolTip::showText(he->globalPos(), QStringLiteral("最小化"), this);
        else if (maximizeBtnRect().contains(he->pos()))
            QToolTip::showText(he->globalPos(), m_maximized ? QStringLiteral("还原") : QStringLiteral("最大化"), this);
        else if (closeBtnRect().contains(he->pos()))
            QToolTip::showText(he->globalPos(), QStringLiteral("关闭"), this);
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}
