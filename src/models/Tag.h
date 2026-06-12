#ifndef TAG_H
#define TAG_H

#include <QString>
#include <QColor>

struct Tag {
    int id = -1;
    QString name;
    QColor color{Qt::gray};

    bool isValid() const { return id >= 0 && !name.isEmpty(); }
};

#endif
