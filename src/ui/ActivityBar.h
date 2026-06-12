#ifndef ACTIVITYBAR_H
#define ACTIVITYBAR_H

#include <QWidget>
#include <QVector>

class ActivityBar : public QWidget
{
    Q_OBJECT
public:
    enum Activity { Gallery = 0, Favorites, Tags, Settings };

    explicit ActivityBar(QWidget *parent = nullptr);

    void setActive(Activity act);

signals:
    void activitySelected(Activity act);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    QRect iconRect(int idx) const;
    int indexAt(const QPoint &pos) const;

    Activity m_active = Gallery;
    int m_hovered = -1;

    struct IconDef {
        QString iconName;
        QString tooltip;
    };
    QVector<IconDef> m_icons;
};

#endif
