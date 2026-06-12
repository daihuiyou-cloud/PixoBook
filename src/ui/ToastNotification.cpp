#include "ToastNotification.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QApplication>

ToastNotification::ToastNotification(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(280, 48);

    m_label = new QLabel();
    m_label->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->addWidget(m_label);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        auto *anim = new QPropertyAnimation(this, "opacity");
        anim->setDuration(300);
        anim->setStartValue(1.0);
        anim->setEndValue(0.0);
        connect(anim, &QPropertyAnimation::finished, this, &QWidget::hide);
        connect(anim, &QPropertyAnimation::finished, this, &QWidget::deleteLater);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void ToastNotification::show(QWidget *parent, const QString &message, int durationMs)
{
    if (!parent) return;
    auto *toast = new ToastNotification(parent);
    toast->showMessage(message, durationMs);
}

void ToastNotification::showMessage(const QString &message, int durationMs)
{
    m_label->setText(message);

    QPoint pos = parentWidget()->mapToGlobal(
        QPoint(parentWidget()->width() - width() - 16, 50));
    move(pos);

    QWidget::show();
    raise();

    m_timer->start(durationMs);
}

void ToastNotification::setOpacity(qreal op)
{
    m_opacity = op;
    update();
}

void ToastNotification::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);
    p.setBrush(QColor(0x25, 0x25, 0x26));
    p.setPen(QPen(QColor(0x00, 0x7a, 0xcc), 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);
}
