#include "ActivityBar.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QToolTip>
#include <QEvent>
#include "Codicon.h"

ActivityBar::ActivityBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(48);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_icons = {
        { "layout",     QStringLiteral("\u56fe\u5e93") },
        { "star",       QStringLiteral("\u6536\u85cf") },
        { "tag",        QStringLiteral("\u6807\u7b7e") },
        { "settings",   QStringLiteral("\u8bbe\u7f6e") },
    };
}

void ActivityBar::setActive(Activity act)
{
    m_active = act;
    update();
}

QRect ActivityBar::iconRect(int idx) const
{
    return QRect(0, idx * 48, 48, 48);
}

int ActivityBar::indexAt(const QPoint &pos) const
{
    if (pos.x() < 0 || pos.x() >= width()) return -1;
    int idx = pos.y() / 48;
    return (idx >= 0 && idx < m_icons.size()) ? idx : -1;
}

void ActivityBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), QColor(0x33, 0x33, 0x33));

    for (int i = 0; i < m_icons.size(); i++) {
        QRect r = iconRect(i);
        bool active = (i == static_cast<int>(m_active));
        bool hovered = (i == m_hovered);

        if (active) {
            p.fillRect(r.left(), r.top(), 3, r.height(), QColor(0x00, 0x7a, 0xcc));
        }

        QColor bg = active ? QColor(0x25, 0x25, 0x26)
                  : hovered ? QColor(0x2a, 0x2d, 0x2e)
                  : Qt::transparent;
        if (bg != Qt::transparent)
            p.fillRect(r.adjusted(3, 0, 0, 0), bg);

        QColor iconColor = active ? QColor(0xff, 0xff, 0xff) : QColor(0x96, 0x96, 0x96);
        Codicon::draw(p, m_icons[i].iconName, r.adjusted(4, 4, -4, -4), iconColor, 24);
    }
}

void ActivityBar::mouseMoveEvent(QMouseEvent *event)
{
    int oldHover = m_hovered;
    m_hovered = indexAt(event->pos());
    if (oldHover != m_hovered) {
        update();
        if (m_hovered >= 0) {
            QToolTip::showText(event->globalPos(), m_icons[m_hovered].tooltip, this);
        } else {
            QToolTip::hideText();
        }
    }
}

void ActivityBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    int idx = indexAt(event->pos());
    if (idx >= 0 && idx != static_cast<int>(m_active)) {
        m_active = static_cast<Activity>(idx);
        update();
        emit activitySelected(m_active);
    }
}

void ActivityBar::keyPressEvent(QKeyEvent *event)
{
    int cur = static_cast<int>(m_active);
    if (event->key() == Qt::Key_Up && cur > 0) {
        m_active = static_cast<Activity>(cur - 1);
        update();
        emit activitySelected(m_active);
    } else if (event->key() == Qt::Key_Down && cur < m_icons.size() - 1) {
        m_active = static_cast<Activity>(cur + 1);
        update();
        emit activitySelected(m_active);
    } else if (event->key() == Qt::Key_Home) {
        m_active = static_cast<Activity>(0);
        update();
        emit activitySelected(m_active);
    } else if (event->key() == Qt::Key_End) {
        m_active = static_cast<Activity>(m_icons.size() - 1);
        update();
        emit activitySelected(m_active);
    } else {
        QWidget::keyPressEvent(event);
    }
}

void ActivityBar::leaveEvent(QEvent *)
{
    m_hovered = -1;
    update();
    QToolTip::hideText();
}

bool ActivityBar::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent *he = static_cast<QHelpEvent *>(event);
        int idx = indexAt(he->pos());
        if (idx >= 0) {
            QToolTip::showText(he->globalPos(), m_icons[idx].tooltip, this);
        }
        return true;
    }
    return QWidget::event(event);
}
