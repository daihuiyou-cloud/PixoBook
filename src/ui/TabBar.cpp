#include "TabBar.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QToolTip>
#include "Codicon.h"
#include "ColorConstants.h"

TabBar::TabBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(kTabHeight);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_tabs = {
        { QStringLiteral("\u6240\u6709\u7d20\u6750"), QStringLiteral("layout") },
        { QStringLiteral("\u6536\u85cf\u5939"), QStringLiteral("star") },
        { QStringLiteral("\u6700\u8fd1\u5bfc\u5165"), QStringLiteral("history") },
    };
}

void TabBar::setCurrentIndex(int idx)
{
    if (idx != m_currentIndex) {
        m_currentIndex = idx;
        emit currentChanged(idx);
        update();
    }
}

void TabBar::setTabs(const QVector<Tab> &tabs)
{
    m_tabs = tabs;
    m_currentIndex = 0;
    update();
}

QRect TabBar::tabRect(int index) const
{
    QFont f = font();
    QFontMetrics fm(f);
    int x = 0;
    for (int i = 0; i < index && i < m_tabs.size(); i++) {
        int labelWidth = fm.horizontalAdvance(m_tabs[i].label);
        int tabWidth = qMax(kTabMinWidth, labelWidth + kTabPadding * 2 + 16);
        x += tabWidth;
    }
    int labelWidth = fm.horizontalAdvance(m_tabs[index].label);
    int width = qMax(kTabMinWidth, labelWidth + kTabPadding * 2 + 16);
    return QRect(x, 0, width, kTabHeight);
}

QRect TabBar::addButtonRect() const
{
    QFont f = font();
    QFontMetrics fm(f);
    int x = 4;
    for (int i = 0; i < m_tabs.size(); i++) {
        int labelWidth = fm.horizontalAdvance(m_tabs[i].label);
        int tabWidth = qMax(kTabMinWidth, labelWidth + kTabPadding * 2 + 16);
        x += tabWidth;
    }
    int y = (kTabHeight - 24) / 2;
    return QRect(x, y, 24, 24);
}

int TabBar::indexAt(const QPoint &pos) const
{
    for (int i = 0; i < m_tabs.size(); i++) {
        if (tabRect(i).contains(pos))
            return i;
    }
    return -1;
}

void TabBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    p.fillRect(rect(), Color::BG_DARK);

    p.fillRect(QRect(0, height() - 1, width(), 1), Color::BORDER);

    for (int i = 0; i < m_tabs.size(); i++) {
        QRect r = tabRect(i);

        if (i == m_currentIndex) {
            p.fillRect(r, Color::BG_DARKEST);
        } else if (i == m_hoveredIndex) {
            p.fillRect(r, Color::BG_HOVER);
        }

        if (i == m_currentIndex) {
            p.fillRect(QRect(r.left(), r.bottom() - 2, r.width(), 2), Color::ACCENT);
        }

        QColor textColor;
        if (i == m_currentIndex) {
            textColor = Color::TEXT_BRIGHT;
        } else if (i == m_hoveredIndex) {
            textColor = Color::TEXT_PRIMARY;
        } else {
            textColor = Color::TEXT_SECONDARY;
        }

        Codicon::draw(p, m_tabs[i].icon, QRect(r.left() + 10, 9, 16, 16), textColor, 14);

        p.setPen(textColor);
        p.drawText(QRect(r.left() + 28, r.top(), r.width() - 28 - kTabPadding, kTabHeight),
                   Qt::AlignVCenter | Qt::AlignLeft, m_tabs[i].label);
    }

    {
        QRect r = addButtonRect();
        if (m_hoveredAdd) {
            p.fillRect(r, Color::BG_SELECTED);
            QToolTip::showText(mapToGlobal(r.center()), QStringLiteral("新建标签页"), this);
        }
        p.setPen(Color::TEXT_PRIMARY);
        p.drawText(r, Qt::AlignCenter, QStringLiteral("+"));
    }
}

void TabBar::mouseMoveEvent(QMouseEvent *event)
{
    int oldHovered = m_hoveredIndex;
    bool oldHoveredAdd = m_hoveredAdd;

    int idx = indexAt(event->pos());
    m_hoveredIndex = idx;
    m_hoveredAdd = addButtonRect().contains(event->pos());

    if (oldHovered != m_hoveredIndex || oldHoveredAdd != m_hoveredAdd) {
        update();
    }
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (addButtonRect().contains(event->pos())) {
        emit addTabRequested();
        return;
    }

    int idx = indexAt(event->pos());
    if (idx >= 0 && idx != m_currentIndex) {
        setCurrentIndex(idx);
    }
}

void TabBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Left && m_currentIndex > 0) {
        setCurrentIndex(m_currentIndex - 1);
    } else if (event->key() == Qt::Key_Right && m_currentIndex < m_tabs.size() - 1) {
        setCurrentIndex(m_currentIndex + 1);
    } else if (event->key() == Qt::Key_Home && !m_tabs.isEmpty()) {
        setCurrentIndex(0);
    } else if (event->key() == Qt::Key_End && !m_tabs.isEmpty()) {
        setCurrentIndex(m_tabs.size() - 1);
    } else {
        QWidget::keyPressEvent(event);
    }
}

void TabBar::leaveEvent(QEvent *)
{
    if (m_hoveredIndex != -1 || m_hoveredAdd) {
        m_hoveredIndex = -1;
        m_hoveredAdd = false;
        update();
    }
}
