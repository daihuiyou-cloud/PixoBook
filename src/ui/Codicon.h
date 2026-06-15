#ifndef CODICON_H
#define CODICON_H

#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QString>
#include <QHash>

class Codicon {
public:
    static void init() {
        QFontDatabase::addApplicationFont(":/codicon.ttf");
    }

    static QFont font(int pixelSize = 22) {
        static QHash<int, QFont> cache;
        auto it = cache.constFind(pixelSize);
        if (it != cache.constEnd())
            return it.value();
        QFont f("codicon");
        f.setPixelSize(pixelSize);
        cache.insert(pixelSize, f);
        return f;
    }

    static QString icon(const QString &name) {
        static const QHash<QString, QString> map = {
            { "add",          QString(QChar(0xea60)) },
            { "plus",         QString(QChar(0xea60)) },
            { "tag",          QString(QChar(0xea66)) },
            { "star",         QString(QChar(0xea6a)) },
            { "history",      QString(QChar(0xea82)) },
            { "search",       QString(QChar(0xea6d)) },
            { "close",        QString(QChar(0xea76)) },
            { "x",            QString(QChar(0xea76)) },
            { "file",         QString(QChar(0xea7b)) },
            { "edit",         QString(QChar(0xea73)) },
            { "pencil",       QString(QChar(0xea73)) },
            { "trash",        QString(QChar(0xea81)) },
            { "delete",       QString(QChar(0xea81)) },
            { "folder",       QString(QChar(0xea83)) },
            { "terminal",     QString(QChar(0xea85)) },
            { "check",        QString(QChar(0xeab2)) },
            { "chevron-down", QString(QChar(0xeab4)) },
            { "chevron-left", QString(QChar(0xeab5)) },
            { "chevron-right",QString(QChar(0xeab6)) },
            { "chevron-up",   QString(QChar(0xeab7)) },
            { "filter",       QString(QChar(0xeaf1)) },
            { "gear",         QString(QChar(0xeaf8)) },
            { "settings",     QString(QChar(0xeb52)) },
            { "home",         QString(QChar(0xeb06)) },
            { "fullscreen",   QString(QChar(0xeb4c)) },
            { "screen-normal",QString(QChar(0xeb4d)) },
            { "image",        QString(QChar(0xeada)) },
            { "camera",       QString(QChar(0xeada)) },
            { "layout",       QString(QChar(0xebeb)) },
            { "symbol-field", QString(QChar(0xeb5f)) },
            { "symbol-misc",  QString(QChar(0xeb63)) },
            { "ellipsis",     QString(QChar(0xea7c)) },
            { "eye",          QString(QChar(0xea70)) },
            { "file-media",   QString(QChar(0xeada)) },
            { "folder-opened",QString(QChar(0xea84)) },
            { "question",     QString(QChar(0xeb32)) },
            { "chrome-close", QString(QChar(0xeab8)) },
            { "chrome-maximize", QString(QChar(0xeab9)) },
            { "chrome-minimize", QString(QChar(0xeaba)) },
            { "chrome-restore", QString(QChar(0xeabb)) },
            { "zoom-in",      QString(QChar(0xeb81)) },
            { "screen-full",  QString(QChar(0xeb4c)) },
            { "star-empty",   QString(QChar(0xea6a)) },
            { "copy",         QString(QChar(0xebcc)) },
            { "files",        QString(QChar(0xeaf0)) },
            { "info",         QString(QChar(0xea74)) },
            { "open-preview", QString(QChar(0xeb28)) },
            { "quote",        QString(QChar(0xeb33)) },
            { "zoom-out",     QString(QChar(0xeb82)) },
            { "arrow-left",   QString(QChar(0xea9b)) },
            { "arrow-right",  QString(QChar(0xea9c)) },
            { "arrow-up",     QString(QChar(0xeaa1)) },
            { "arrow-down",   QString(QChar(0xea9a)) },
            { "clear-all",    QString(QChar(0xeabf)) },
        };
        return map.value(name, QString(QChar(0xea7b)));
    }

    static void draw(QPainter &p, const QString &iconName, const QRect &r,
                     const QColor &color = QColor(0xcc, 0xcc, 0xcc), int size = 16) {
        p.setFont(font(size));
        p.setPen(color);
        p.drawText(r, Qt::AlignCenter, icon(iconName));
    }

    static QIcon cachedIcon(const QString &name, const QColor &color,
                            int pixelSize, int w = 16, int h = 16) {
        QString key = name + '|' + QString::number(color.rgba()) + '|'
                      + QString::number(pixelSize) + '|' + QString::number(w) + '|' + QString::number(h);
        static QHash<QString, QIcon> cache;
        auto it = cache.constFind(key);
        if (it != cache.constEnd())
            return it.value();
        QPixmap px(w, h);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        draw(p, name, px.rect(), color, pixelSize);
        cache.insert(key, QIcon(px));
        return cache.value(key);
    }
};

#endif
