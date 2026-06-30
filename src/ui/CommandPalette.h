#ifndef COMMANDPALETTE_H
#define COMMANDPALETTE_H

#include <QWidget>
#include <QLineEdit>
#include <QVector>
#include <QTimer>
#include <functional>

class CommandPalette : public QWidget
{
    Q_OBJECT
public:
    struct Command {
        QString label;
        QString shortcut;
        std::function<void()> action;
    };

    explicit CommandPalette(QWidget *parent);
    void show(const QVector<Command> &commands);

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void ensureVisible();

    QLineEdit *m_input;
    QVector<Command> m_commands;
    QVector<int> m_filtered;
    QVector<QVector<int>> m_matchPositions;
    int m_selectedIdx = 0;
    int m_scrollOffset = 0;
    QTimer *m_debounceTimer;
    QString m_pendingText;
    void rebuildFilter();
    void drawHighlightedText(QPainter &p, const QRect &rect, const QString &text,
                             const QVector<int> &positions) const;
};

#endif
