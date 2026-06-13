#include "ActivityBar.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QToolTip>
#include <QEvent>
#include <QHelpEvent>
#include "Codicon.h"
#include "ColorConstants.h"
#include "VisualConstants.h"

ActivityBar::ActivityBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(48);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_icons = {
        { "layout",     QStringLiteral("图库") },
        { "star",       QStringLiteral("收藏") },
        { "tag",        QStringLiteral("标签") },
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

    p.fillRect(rect(), Color::BG_ACTIVITYBAR);
    p.fillRect(QRect(width() - 1, 0, 1, height()), Color::BORDER);

    for (int i = 0; i < m_icons.size(); i++) {
        QRect r = iconRect(i);
        bool active = (i == static_cast<int>(m_active));
        bool hovered = (i == m_hovered);

        if (active)
            p.fillRect(QRect(r.left(), r.top() + 8, 3, r.height() - 16), Color::ACCENT);
        else if (hovered) {
            p.setBrush(Color::BG_HOVER);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r.adjusted(4, 6, -4, -6), Visual::RadiusSmall, Visual::RadiusSmall);
        }

        QColor iconColor = active ? Color::TEXT_BRIGHT
                         : hovered ? Color::TEXT_PRIMARY
                         : Color::TEXT_SECONDARY;
        Codicon::draw(p, m_icons[i].iconName, r.adjusted(4, 4, -4, -4), iconColor, 23);
    }
}

void ActivityBar::mouseMoveEvent(QMouseEvent *event)
{
    int oldHover = m_hovered;
    m_hovered = indexAt(event->pos());
    if (oldHover != m_hovered)
        update();
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
