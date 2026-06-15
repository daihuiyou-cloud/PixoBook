#include "TabBar.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include "Codicon.h"
#include "ColorConstants.h"
#include "VisualConstants.h"

TabBar::TabBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(kTabHeight);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    QFont base = font();
    m_tabFont = base; m_tabFont.setPixelSize(Visual::FontControl);

    m_tabs = {
        { tr("所有素材"), QStringLiteral("layout") },
        { tr("收藏夹"), QStringLiteral("star") },
        { tr("最近导入"), QStringLiteral("history") },
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
    f.setPixelSize(Visual::FontControl);
    QFontMetrics fm(f);
    int x = 0;
    for (int i = 0; i < index && i < m_tabs.size(); i++) {
        int labelWidth = fm.horizontalAdvance(m_tabs[i].label);
        int tabWidth = qMax(kTabMinWidth, labelWidth + kTabPadding * 2 + 28);
        x += tabWidth;
    }
    int labelWidth = fm.horizontalAdvance(m_tabs[index].label);
    int width = qMax(kTabMinWidth, labelWidth + kTabPadding * 2 + 28);
    return QRect(x, 0, width, kTabHeight);
}

QRect TabBar::addButtonRect() const
{
    QFont f = font();
    f.setPixelSize(Visual::FontControl);
    QFontMetrics fm(f);
    int x = 6;
    for (int i = 0; i < m_tabs.size(); i++) {
        int labelWidth = fm.horizontalAdvance(m_tabs[i].label);
        x += qMax(kTabMinWidth, labelWidth + kTabPadding * 2 + 28);
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
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Color::BG_DARK);
    p.fillRect(QRect(0, height() - 1, width(), 1), Color::BORDER);

    p.setFont(m_tabFont);

    for (int i = 0; i < m_tabs.size(); i++) {
        QRect r = tabRect(i);
        bool active = i == m_currentIndex;

        if (active)
            p.fillRect(r, Color::BG_DARKEST);
        else if (i == m_hoveredIndex)
            p.fillRect(r, Color::BG_HOVER);

        if (active)
            p.fillRect(QRect(r.left(), r.bottom() - 2, r.width(), 2), Color::ACCENT);

        QColor textColor = active ? Color::TEXT_BRIGHT
                         : (i == m_hoveredIndex ? Color::TEXT_PRIMARY : Color::TEXT_SECONDARY);
        Codicon::draw(p, m_tabs[i].icon, QRect(r.left() + 12, 0, 16, kTabHeight), textColor, 14);
        p.setPen(textColor);
        p.drawText(QRect(r.left() + 34, r.top(), r.width() - 34 - kTabPadding, kTabHeight),
                   Qt::AlignVCenter | Qt::AlignLeft, m_tabs[i].label);
    }

    QRect r = addButtonRect();
    if (m_hoveredAdd) {
        p.setBrush(Color::BG_HOVER);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(r.adjusted(2, 2, -2, -2), Visual::RadiusSmall, Visual::RadiusSmall);
        p.setBrush(Qt::NoBrush);
    }
    Codicon::draw(p, "add", r, Color::TEXT_PRIMARY, 14);
}

void TabBar::mouseMoveEvent(QMouseEvent *event)
{
    int oldHovered = m_hoveredIndex;
    bool oldHoveredAdd = m_hoveredAdd;

    m_hoveredIndex = indexAt(event->pos());
    m_hoveredAdd = addButtonRect().contains(event->pos());

    if (m_hoveredAdd)
        QToolTip::showText(mapToGlobal(addButtonRect().center()), tr("导入素材文件夹"), this);
    else if (oldHoveredAdd)
        QToolTip::hideText();

    if (oldHovered != m_hoveredIndex || oldHoveredAdd != m_hoveredAdd)
        update();
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (addButtonRect().contains(event->pos())) {
        emit addTabRequested();
        return;
    }

    int idx = indexAt(event->pos());
    if (idx >= 0 && idx != m_currentIndex)
        setCurrentIndex(idx);
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
        QToolTip::hideText();
    }
}
