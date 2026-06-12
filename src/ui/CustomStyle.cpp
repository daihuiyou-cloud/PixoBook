#include "CustomStyle.h"
#include "ColorConstants.h"
#include <QPainter>
#include <QStyleOption>
#include <QStyleOptionButton>
#include <QStyleOptionComboBox>
#include <QStyleOptionSlider>
#include <QStyleOptionMenuItem>
#include <QStyleOptionToolBox>
#include <QStyleOptionProgressBar>
#include <QProgressBar>
#include <QApplication>
#include <QPainterPath>

CustomStyle::CustomStyle()
    : QProxyStyle("Fusion")
{
}

QPalette CustomStyle::standardPalette() const
{
    QPalette p;
    p.setColor(QPalette::Window, Color::BG_DARKEST);
    p.setColor(QPalette::WindowText, Color::TEXT_PRIMARY);
    p.setColor(QPalette::Base, Color::BG_INPUT);
    p.setColor(QPalette::AlternateBase, Color::BG_DARK);
    p.setColor(QPalette::ToolTipBase, Color::BG_DARK);
    p.setColor(QPalette::ToolTipText, Color::TEXT_PRIMARY);
    p.setColor(QPalette::Text, Color::TEXT_PRIMARY);
    p.setColor(QPalette::Button, Color::BG_INPUT);
    p.setColor(QPalette::ButtonText, Color::TEXT_PRIMARY);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, Color::HIGHLIGHT);
    p.setColor(QPalette::HighlightedText, Color::TEXT_PRIMARY);
    p.setColor(QPalette::Light, Color::BG_BUTTON_HOVER);
    p.setColor(QPalette::Midlight, Color::BG_INPUT);
    p.setColor(QPalette::Mid, Color::BG_MENUBAR);
    p.setColor(QPalette::Dark, Color::BG_DARK);
    p.setColor(QPalette::Shadow, Color::BG_DARKEST);
    p.setColor(QPalette::Disabled, QPalette::WindowText, Color::TEXT_SECONDARY);
    p.setColor(QPalette::Disabled, QPalette::Text, Color::TEXT_SECONDARY);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, Color::TEXT_SECONDARY);
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

        painter->setBrush(Color::BG_INPUT);
        painter->setPen(QPen(hasFocus ? Color::ACCENT : Color::BORDER_INPUT, 1));
        painter->drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
        return;
    }
    case PE_PanelTipLabel: {
        QRect r = option->rect;
        painter->setBrush(Color::BG_DARK);
        painter->setPen(QPen(Color::BORDER, 1));
        painter->drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
        return;
    }
    case PE_PanelMenu: {
        painter->fillRect(option->rect, Color::BG_DARK);
        return;
    }
    case PE_FrameMenu: {
        QRect r = option->rect;
        painter->setBrush(Color::BG_DARK);
        painter->setPen(QPen(Color::BORDER, 1));
        painter->drawRect(r.adjusted(0, 0, -1, -1));
        return;
    }
    case PE_PanelButtonCommand: {
        auto *btnOpt = qstyleoption_cast<const QStyleOptionButton *>(option);
        QRect r = option->rect;
        bool hovered = option->state & State_MouseOver;
        bool pressed = option->state & State_Sunken;
        bool checked = option->state & State_On;
        Q_UNUSED(btnOpt);

        QColor bg;
        if (checked) bg = Color::BG_BUTTON_CHECKED;
        else if (pressed) bg = Color::BG_BUTTON_HOVER;
        else if (hovered) bg = Color::BG_BUTTON_HOVER;
        else bg = Color::BG_BUTTON;

        painter->setBrush(bg);
        painter->setPen(QPen(checked ? Color::FOCUS_BORDER : Color::BORDER_SUBTLE, 1));
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
        bool enabled = btnOpt->state & State_Enabled;
        bool hasFocus = btnOpt->state & State_HasFocus;
        bool segmented = widget && widget->property("segmented").toBool();
        bool toolbarButton = widget && widget->property("toolbarButton").toBool();

        QColor bg;
        if (checked) bg = toolbarButton ? Color::BG_BUTTON_SUBTLE : Color::BG_BUTTON_CHECKED;
        else if (pressed) bg = Color::BG_BUTTON_HOVER;
        else if (hovered) bg = Color::BG_BUTTON_HOVER;
        else bg = toolbarButton ? Color::BG_DARK : Color::BG_BUTTON;

        QColor border = hasFocus ? Color::FOCUS_BORDER : (checked ? Color::ACCENT : Color::BORDER_SUBTLE);

        painter->setBrush(bg);
        painter->setPen(QPen(border, 1));

        QRect frame = pressed ? r.adjusted(2, 2, -1, -1) : r.adjusted(1, 1, -1, -1);
        if (segmented) {
            QString position = widget->property("segmentPosition").toString();
            QPainterPath path;
            const qreal radius = 4.0;
            QRectF rf(frame);
            if (position == "first") {
                path.moveTo(rf.right(), rf.top());
                path.lineTo(rf.right(), rf.bottom());
                path.lineTo(rf.left() + radius, rf.bottom());
                path.quadTo(rf.left(), rf.bottom(), rf.left(), rf.bottom() - radius);
                path.lineTo(rf.left(), rf.top() + radius);
                path.quadTo(rf.left(), rf.top(), rf.left() + radius, rf.top());
                path.closeSubpath();
            } else if (position == "last") {
                path.moveTo(rf.left(), rf.top());
                path.lineTo(rf.right() - radius, rf.top());
                path.quadTo(rf.right(), rf.top(), rf.right(), rf.top() + radius);
                path.lineTo(rf.right(), rf.bottom() - radius);
                path.quadTo(rf.right(), rf.bottom(), rf.right() - radius, rf.bottom());
                path.lineTo(rf.left(), rf.bottom());
                path.closeSubpath();
            } else {
                path.addRect(rf);
            }
            painter->drawPath(path);
        } else {
            painter->drawRoundedRect(frame, 4, 4);
        }

        QColor contentColor = !enabled ? Color::TEXT_DISABLED
                            : (toolbarButton && checked) ? Color::FAVORITE_ON
                            : checked ? Color::TEXT_BRIGHT
                            : Color::TEXT_PRIMARY;
        QSize iconSize = btnOpt->iconSize.isValid() ? btnOpt->iconSize : QSize(16, 16);
        bool hasIcon = !btnOpt->icon.isNull();
        bool hasText = !btnOpt->text.isEmpty();
        int gap = hasIcon && hasText ? 6 : 0;
        int iconW = hasIcon ? iconSize.width() : 0;
        int textW = hasText ? btnOpt->fontMetrics.horizontalAdvance(btnOpt->text) : 0;
        int totalW = iconW + gap + textW;
        int x = r.center().x() - totalW / 2;

        if (hasIcon) {
            QRect iconRect(x, r.center().y() - iconSize.height() / 2, iconSize.width(), iconSize.height());
            QIcon::Mode mode = enabled ? QIcon::Normal : QIcon::Disabled;
            QIcon::State state = checked ? QIcon::On : QIcon::Off;
            btnOpt->icon.paint(painter, iconRect, Qt::AlignCenter, mode, state);
            x += iconSize.width() + gap;
        }
        if (hasText) {
            painter->setPen(contentColor);
            painter->drawText(QRect(x, r.top(), textW + 1, r.height()), Qt::AlignVCenter | Qt::AlignLeft, btnOpt->text);
        }
        return;
    }
    case CE_MenuBarEmptyArea: {
        painter->fillRect(option->rect, Color::BG_MENUBAR);
        return;
    }
    case CE_MenuBarItem: {
        auto *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (!menuItem) break;

        QRect r = menuItem->rect;
        bool hovered = menuItem->state & State_Selected;

        if (hovered)
            painter->fillRect(r, Color::HIGHLIGHT);

        painter->setPen(Color::TEXT_PRIMARY);
        painter->drawText(r.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                          menuItem->text);
        return;
    }
    case CE_MenuItem: {
        auto *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (!menuItem) break;

        QRect r = menuItem->rect;
        bool selected = menuItem->state & State_Selected;
        bool disabled = !(menuItem->state & State_Enabled);
        bool checked = menuItem->checked;
        bool isCheckable = menuItem->checkType != QStyleOptionMenuItem::NotCheckable;

        if (menuItem->menuItemType == QStyleOptionMenuItem::Separator) {
            painter->fillRect(r, Color::BG_DARK);
            painter->setPen(Color::BORDER);
            painter->drawLine(r.left() + 8, r.center().y(), r.right() - 8, r.center().y());
            return;
        }

        painter->fillRect(r, Color::BG_DARK);
        if (selected)
            painter->fillRect(r, Color::HIGHLIGHT);

        QColor textColor = disabled ? Color::TEXT_DISABLED : Color::TEXT_PRIMARY;
        painter->setPen(textColor);

        int leftMargin = 12;

        if (isCheckable) {
            QRect checkRect(r.left() + 4, r.top(), 20, r.height());
            painter->setPen(Color::TEXT_SECONDARY);
            painter->drawRect(QRect(checkRect.center().x() - 5, checkRect.center().y() - 5, 10, 10));
            if (checked) {
                painter->setPen(QPen(Color::FAVORITE_ON, 2));
                QPolygon check;
                check << QPoint(checkRect.center().x() - 3, checkRect.center().y() - 1)
                      << QPoint(checkRect.center().x() - 1, checkRect.center().y() + 3)
                      << QPoint(checkRect.center().x() + 4, checkRect.center().y() - 3);
                painter->drawPolyline(check);
            }
            leftMargin = 28;
        }

        int iconWidth = 0;
        if (!menuItem->icon.isNull() && !isCheckable)
            iconWidth = menuItem->icon.availableSizes().value(0, QSize(16, 16)).width() + 6;

        QRect textRect = r.adjusted(leftMargin + iconWidth, 0, -30, 0);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          menuItem->fontMetrics.elidedText(menuItem->text, Qt::ElideRight, textRect.width()));

        if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu)
            painter->drawText(r.adjusted(0, 0, -8, 0), Qt::AlignVCenter | Qt::AlignRight, QString(QChar(0x25B6)));
        return;
    }
    case CE_MenuEmptyArea: {
        painter->fillRect(option->rect, Color::BG_DARK);
        return;
    }
    case CE_Splitter: {
        QRect r = option->rect;
        painter->setPen(QPen(Color::BORDER, 1));
        if (r.width() > r.height())
            painter->drawLine(r.left(), r.center().y(), r.right(), r.center().y());
        else
            painter->drawLine(r.center().x(), r.top(), r.center().x(), r.bottom());
        return;
    }
    case CE_ProgressBarGroove: {
        QRect r = option->rect;
        painter->setBrush(Color::BG_INPUT);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(r, 3, 3);
        return;
    }
    case CE_ProgressBarContents: {
        auto *pbOpt = qstyleoption_cast<const QStyleOptionProgressBar *>(option);
        if (!pbOpt) break;
        QRect r = option->rect.adjusted(1, 1, -1, -1);
        double progress = qBound(0.0, pbOpt->progress / 100.0, 1.0);
        if (progress > 0) {
            int fillW = qMax(4, (int)(r.width() * progress));
            painter->setBrush(Color::ACCENT);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(QRect(r.left(), r.top(), fillW, r.height()), 2, 2);
        }
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
            painter->setBrush(hovered ? Color::SCROLLBAR_HOVER : Color::SCROLLBAR);
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

        QColor borderColor = hasFocus ? Color::FOCUS_BORDER : Color::BORDER_SUBTLE;
        QColor bgColor = pressed ? Color::BG_BUTTON_HOVER : (hovered ? Color::BG_BUTTON_HOVER : Color::BG_INPUT);

        painter->setBrush(bgColor);
        painter->setPen(QPen(borderColor, 1));
        painter->drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);

        if (!cbOpt->currentText.isEmpty()) {
            painter->setPen(Color::TEXT_PRIMARY);
            QRect textRect = r.adjusted(10, 0, -22, 0);
            painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                              painter->fontMetrics().elidedText(cbOpt->currentText, Qt::ElideRight, textRect.width()));
        }

        QRect arrowRect = subControlRect(CC_ComboBox, option, SC_ComboBoxArrow, widget);
        if (arrowRect.isValid() && !arrowRect.isEmpty()) {
            painter->setPen(Color::TEXT_SECONDARY);
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
