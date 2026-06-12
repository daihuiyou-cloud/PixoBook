#include "CustomStyle.h"
#include <QPainter>
#include <QStyleOption>
#include <QStyleOptionButton>
#include <QStyleOptionComboBox>
#include <QStyleOptionSlider>
#include <QStyleOptionMenuItem>
#include <QStyleOptionToolBox>
#include <QApplication>

CustomStyle::CustomStyle()
    : QProxyStyle("Fusion")
{
}

QPalette CustomStyle::standardPalette() const
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
    p.setColor(QPalette::WindowText, QColor(0xcc, 0xcc, 0xcc));
    p.setColor(QPalette::Base, QColor(0x3c, 0x3c, 0x3c));
    p.setColor(QPalette::AlternateBase, QColor(0x25, 0x25, 0x26));
    p.setColor(QPalette::ToolTipBase, QColor(0x25, 0x25, 0x26));
    p.setColor(QPalette::ToolTipText, QColor(0xcc, 0xcc, 0xcc));
    p.setColor(QPalette::Text, QColor(0xcc, 0xcc, 0xcc));
    p.setColor(QPalette::Button, QColor(0x3c, 0x3c, 0x3c));
    p.setColor(QPalette::ButtonText, QColor(0xcc, 0xcc, 0xcc));
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
    p.setColor(QPalette::HighlightedText, QColor(0xcc, 0xcc, 0xcc));
    p.setColor(QPalette::Light, QColor(0x4a, 0x4a, 0x4a));
    p.setColor(QPalette::Midlight, QColor(0x3c, 0x3c, 0x3c));
    p.setColor(QPalette::Mid, QColor(0x32, 0x32, 0x33));
    p.setColor(QPalette::Dark, QColor(0x25, 0x25, 0x26));
    p.setColor(QPalette::Shadow, QColor(0x1e, 0x1e, 0x1e));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x96, 0x96, 0x96));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x96, 0x96, 0x96));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x96, 0x96, 0x96));
    return p;
}

int CustomStyle::pixelMetric(PixelMetric metric, const QStyleOption *option,
                             const QWidget *widget) const
{
    switch (metric) {
    case PM_ScrollBarExtent:
        return 10;
    case PM_ScrollBarSliderMin:
        return 30;
    case PM_SplitterWidth:
        return 1;
    case PM_MenuVMargin:
    case PM_MenuHMargin:
        return 0;
    case PM_MenuPanelWidth:
        return 1;
    case PM_ButtonIconSize:
        return 16;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

QSize CustomStyle::sizeFromContents(ContentsType type, const QStyleOption *option,
                                    const QSize &contentsSize, const QWidget *widget) const
{
    QSize sz = QProxyStyle::sizeFromContents(type, option, contentsSize, widget);
    if (type == CT_MenuItem) {
        sz.setHeight(qMax(sz.height(), 24));
    }
    return sz;
}

QRect CustomStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                                  SubControl subControl, const QWidget *widget) const
{
    switch (control) {
    case CC_ScrollBar:
        if (subControl == SC_ScrollBarSubLine || subControl == SC_ScrollBarAddLine)
            return QRect();
        break;
    case CC_ComboBox:
        if (subControl == SC_ComboBoxArrow) {
            const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(option);
            if (cb)
                return QRect(cb->rect.right() - 20, cb->rect.top(), 20, cb->rect.height());
            return QRect();
        }
        break;
    default:
        break;
    }
    return QProxyStyle::subControlRect(control, option, subControl, widget);
}

void CustomStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                                QPainter *painter, const QWidget *widget) const
{
    painter->setRenderHint(QPainter::Antialiasing);

    switch (element) {
    case PE_PanelLineEdit: {
        auto *lineEditOpt = qstyleoption_cast<const QStyleOptionFrame *>(option);
        if (!lineEditOpt) break;

        QRect r = lineEditOpt->rect;
        bool hasFocus = lineEditOpt->state & State_HasFocus;

        painter->setBrush(QColor(0x3c, 0x3c, 0x3c));
        painter->setPen(QPen(hasFocus ? QColor(0x00, 0x7a, 0xcc) : QColor(0x5a, 0x5a, 0x5a), 1));
        painter->drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
        return;
    }
    case PE_PanelTipLabel: {
        QRect r = option->rect;
        painter->setBrush(QColor(0x25, 0x25, 0x26));
        painter->setPen(QPen(QColor(0x3c, 0x3c, 0x3c), 1));
        painter->drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
        return;
    }
    case PE_FrameMenu: {
        QRect r = option->rect;
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(QColor(0x3c, 0x3c, 0x3c), 1));
        painter->drawRect(r.adjusted(0, 0, -1, -1));
        return;
    }
    case PE_PanelButtonCommand: {
        auto *btnOpt = qstyleoption_cast<const QStyleOptionButton *>(option);
        QRect r = option->rect;
        bool hovered = option->state & State_MouseOver;
        bool pressed = option->state & State_Sunken;
        bool isDefault = btnOpt ? (btnOpt->features & QStyleOptionButton::DefaultButton) : false;

        QColor bg;
        if (pressed) bg = QColor(0x4a, 0x4a, 0x4a);
        else if (hovered) bg = QColor(0x4a, 0x4a, 0x4a);
        else bg = QColor(0x3c, 0x3c, 0x3c);

        painter->setBrush(bg);
        painter->setPen(QPen(QColor(0x5a, 0x5a, 0x5a), 1));
        painter->drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
        return;
    }
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void CustomStyle::drawControl(ControlElement element, const QStyleOption *option,
                              QPainter *painter, const QWidget *widget) const
{
    painter->setRenderHint(QPainter::Antialiasing);

    switch (element) {
    case CE_PushButton: {
        auto *btnOpt = qstyleoption_cast<const QStyleOptionButton *>(option);
        if (!btnOpt) break;

        QRect r = btnOpt->rect;
        bool hovered = btnOpt->state & State_MouseOver;
        bool pressed = btnOpt->state & State_Sunken;
        bool checked = btnOpt->state & State_On;

        QColor bg;
        if (checked) bg = QColor(0x09, 0x47, 0x71);
        else if (pressed) bg = QColor(0x4a, 0x4a, 0x4a);
        else if (hovered) bg = QColor(0x4a, 0x4a, 0x4a);
        else bg = QColor(0x3c, 0x3c, 0x3c);

        QColor border = checked ? QColor(0x00, 0x7a, 0xcc) : QColor(0x5a, 0x5a, 0x5a);

        painter->setBrush(bg);
        painter->setPen(QPen(border, 1));
        painter->drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);

        painter->setPen(checked ? Qt::white : QColor(0xcc, 0xcc, 0xcc));
        QFont f = painter->font();
        f.setPixelSize(14);
        painter->setFont(f);
        painter->drawText(r, Qt::AlignCenter, btnOpt->text);
        return;
    }
    case CE_MenuBarEmptyArea: {
        painter->fillRect(option->rect, QColor(0x32, 0x32, 0x33));
        return;
    }
    case CE_MenuBarItem: {
        auto *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (!menuItem) break;

        QRect r = menuItem->rect;
        bool hovered = menuItem->state & State_Selected;

        if (hovered)
            painter->fillRect(r, QColor(0x09, 0x47, 0x71));

        painter->setPen(QColor(0xcc, 0xcc, 0xcc));
        QFont f = menuItem->font;
        f.setPixelSize(14);
        painter->setFont(f);
        painter->drawText(r.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                          menuItem->text);
        return;
    }
    case CE_MenuItem: {
        auto *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (!menuItem) break;

        QRect r = menuItem->rect;
        bool selected = menuItem->state & State_Selected;

        if (menuItem->menuItemType == QStyleOptionMenuItem::Separator) {
            int midY = r.center().y();
            painter->setPen(QColor(0x3c, 0x3c, 0x3c));
            painter->drawLine(r.left() + 8, midY, r.right() - 8, midY);
            return;
        }

        if (selected)
            painter->fillRect(r, QColor(0x09, 0x47, 0x71));

        painter->setPen(QColor(0xcc, 0xcc, 0xcc));
        QFont f = menuItem->font;
        f.setPixelSize(14);
        painter->setFont(f);

        int iconWidth = 0;
        if (!menuItem->icon.isNull())
            iconWidth = menuItem->icon.availableSizes().value(0, QSize(16, 16)).width() + 6;

        QRect textRect = r.adjusted(12 + iconWidth, 0, -30, 0);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          menuItem->fontMetrics.elidedText(menuItem->text, Qt::ElideRight, textRect.width()));

        if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu) {
            painter->drawText(r.adjusted(0, 0, -8, 0), Qt::AlignVCenter | Qt::AlignRight, QString(QChar(0x25B6)));
        }
        return;
    }
    case CE_MenuEmptyArea: {
        painter->fillRect(option->rect, QColor(0x25, 0x25, 0x26));
        return;
    }
    case CE_Splitter: {
        QRect r = option->rect;
        painter->setPen(QPen(QColor(0x3c, 0x3c, 0x3c), 1));
        if (r.width() > r.height())
            painter->drawLine(r.left(), r.center().y(), r.right(), r.center().y());
        else
            painter->drawLine(r.center().x(), r.top(), r.center().x(), r.bottom());
        return;
    }
    default:
        break;
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void CustomStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                     QPainter *painter, const QWidget *widget) const
{
    painter->setRenderHint(QPainter::Antialiasing);

    if (control == CC_ScrollBar) {
        auto *scrollbar = qstyleoption_cast<const QStyleOptionSlider *>(option);
        if (!scrollbar) return;

        bool horizontal = scrollbar->orientation == Qt::Horizontal;
        QRect scrollRect = scrollbar->rect;
        painter->fillRect(scrollRect, Qt::transparent);

        QRect sliderRect = subControlRect(CC_ScrollBar, option, SC_ScrollBarSlider, widget);
        if (sliderRect.isValid() && !sliderRect.isEmpty()) {
            bool hovered = (scrollbar->activeSubControls & SC_ScrollBarSlider);
            painter->setBrush(hovered ? QColor(0x4f, 0x4f, 0x4f) : QColor(0x42, 0x42, 0x42));
            painter->setPen(Qt::NoPen);
            int radius = horizontal ? (sliderRect.height() / 2) : (sliderRect.width() / 2);
            painter->drawRoundedRect(sliderRect, radius, radius);
        }
        return;
    }

    if (control == CC_ComboBox) {
        auto *cbOpt = qstyleoption_cast<const QStyleOptionComboBox *>(option);
        if (!cbOpt) return;

        QRect r = cbOpt->rect;
        bool hovered = cbOpt->state & State_MouseOver;
        bool pressed = cbOpt->state & State_Sunken;
        bool hasFocus = cbOpt->state & State_HasFocus;

        QColor borderColor = hasFocus ? QColor(0x00, 0x7a, 0xcc) : QColor(0x5a, 0x5a, 0x5a);
        QColor bgColor = pressed ? QColor(0x4a, 0x4a, 0x4a) : (hovered ? QColor(0x4a, 0x4a, 0x4a) : QColor(0x3c, 0x3c, 0x3c));

        painter->setBrush(bgColor);
        painter->setPen(QPen(borderColor, 1));
        painter->drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);

        if (!cbOpt->currentText.isEmpty()) {
            painter->setPen(QColor(0xcc, 0xcc, 0xcc));
            QFont f = painter->font();
            f.setPixelSize(14);
            painter->setFont(f);
            QRect textRect = r.adjusted(10, 0, -22, 0);
            painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                              painter->fontMetrics().elidedText(cbOpt->currentText, Qt::ElideRight, textRect.width()));
        }

        QRect arrowRect = subControlRect(CC_ComboBox, option, SC_ComboBoxArrow, widget);
        if (arrowRect.isValid() && !arrowRect.isEmpty()) {
            painter->setPen(QColor(0x96, 0x96, 0x96));
            QPoint center = arrowRect.center();
            QPolygon arrow;
            arrow << QPoint(center.x() - 3, center.y() - 1)
                  << QPoint(center.x() + 3, center.y() - 1)
                  << QPoint(center.x(), center.y() + 2);
            painter->setBrush(Qt::NoBrush);
            painter->drawPolygon(arrow);
        }
        return;
    }

    QProxyStyle::drawComplexControl(control, option, painter, widget);
}
