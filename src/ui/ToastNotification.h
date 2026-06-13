#ifndef TOASTNOTIFICATION_H
#define TOASTNOTIFICATION_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>

class ToastNotification : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
public:
    static void show(QWidget *parent, const QString &message, int durationMs = 3000);

    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal op);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    explicit ToastNotification(QWidget *parent);
    void showMessage(const QString &message, int durationMs);
    void reposition();

    qreal m_opacity = 1.0;
    QLabel *m_label;
    QTimer *m_timer;
};

#endif
