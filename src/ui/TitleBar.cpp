#include "TitleBar.h"
#include <QApplication>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include "Codicon.h"
#include "ColorConstants.h"
#include "VisualConstants.h"

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(kHeight);
    setMouseTracking(true);

    m_menus = {
        { QStringLiteral("文件"), "file" },
        { QStringLiteral("编辑"), "edit" },
        { QStringLiteral("查看"), "eye" },
        { QStringLiteral("帮助"), "question" },
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
    f.setPixelSize(Visual::FontControl);
    QFontMetrics fm(f);
    int x = 118;
    for (int i = 0; i < idx && i < m_menus.size(); i++)
        x += fm.horizontalAdvance(m_menus[i].text) + 38;

    int labelWidth = fm.horizontalAdvance(m_menus[idx].text);
    return QRect(x, 0, labelWidth + 38, kHeight);
}

QRect TitleBar::minimizeBtnRect() const { return QRect(width() - 138, 0, 46, kHeight); }
QRect TitleBar::maximizeBtnRect() const { return QRect(width() - 92, 0, 46, kHeight); }
QRect TitleBar::closeBtnRect() const { return QRect(width() - 46, 0, 46, kHeight); }

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

    Codicon::draw(p, "image", QRect(8, 5, 24, 22), Color::TEXT_PRIMARY, 18);

    QFont f = font();
    f.setPixelSize(Visual::FontControl);
    p.setFont(f);
    p.setPen(Color::TEXT_PRIMARY);
    p.drawText(QRect(36, 0, 78, kHeight), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("AI 素材库"));

    for (int i = 0; i < m_menus.size(); i++) {
        QRect r = menuItemRect(i);
        if (i == m_hoveredMenu)
            p.fillRect(r.adjusted(2, 3, -2, -3), Color::BG_SELECTED);

        QColor textColor = i == m_hoveredMenu ? Color::TEXT_BRIGHT : Color::TEXT_PRIMARY;
        Codicon::draw(p, m_menus[i].iconName, QRect(r.left() + 8, r.top(), 16, r.height()),
                      textColor, 14);
        p.setFont(f);
        p.setPen(textColor);
        p.drawText(r.adjusted(28, 0, 0, 0), Qt::AlignVCenter, m_menus[i].text);
    }

    QRect minRect = minimizeBtnRect();
    if (m_hoveredControl == 0) p.fillRect(minRect, Color::BG_SELECTED);
    Codicon::draw(p, "chrome-minimize", minRect, Color::TEXT_PRIMARY, 14);

    QRect maxRect = maximizeBtnRect();
    if (m_hoveredControl == 1) p.fillRect(maxRect, Color::BG_SELECTED);
    Codicon::draw(p, m_maximized ? "chrome-restore" : "chrome-maximize", maxRect,
                  Color::TEXT_PRIMARY, 14);

    QRect closeRect = closeBtnRect();
    if (m_hoveredControl == 2) {
        p.fillRect(closeRect, Color::CLOSE_HOVER);
        Codicon::draw(p, "chrome-close", closeRect, Qt::white, 14);
    } else {
        Codicon::draw(p, "chrome-close", closeRect, Color::TEXT_PRIMARY, 14);
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

    if (ht == HitMenu) {
        for (int i = 0; i < m_menus.size(); i++) {
            if (menuItemRect(i).contains(event->pos())) {
                m_hoveredMenu = i;
                break;
            }
        }
    } else if (ht == HitMinimize) {
        m_hoveredControl = 0;
    } else if (ht == HitMaximize) {
        m_hoveredControl = 1;
    } else if (ht == HitClose) {
        m_hoveredControl = 2;
    }

    if (oldHoverMenu != m_hoveredMenu || oldHoverControl != m_hoveredControl)
        update();
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
