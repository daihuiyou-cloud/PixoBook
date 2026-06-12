#include "CommandPalette.h"
#include <QPainter>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QApplication>

CommandPalette::CommandPalette(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(500, 320);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Dialog);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_input = new QLineEdit();
    m_input->setPlaceholderText(QString::fromUtf8("\xe8\xbe\x93\xe5\x85\xa5\xe5\x91\xbd\xe4\xbb\xa4\xe5\x90\x8d..."));
    m_input->setFixedHeight(36);
    QPalette inputPal;
    inputPal.setColor(QPalette::Base, QColor(0x3c, 0x3c, 0x3c));
    inputPal.setColor(QPalette::Text, QColor(0xcc, 0xcc, 0xcc));
    m_input->setPalette(inputPal);
    m_input->setTextMargins(12, 0, 12, 0);
    layout->addWidget(m_input);

    connect(m_input, &QLineEdit::textChanged, this, [this]() {
        QString text = m_input->text().toLower();
        m_filtered.clear();
        for (int i = 0; i < m_commands.size(); i++) {
            if (m_commands[i].label.toLower().contains(text))
                m_filtered.append(i);
        }
        m_selectedIdx = m_filtered.isEmpty() ? -1 : 0;
        update();
    });

    connect(m_input, &QLineEdit::returnPressed, this, [this]() {
        if (m_selectedIdx >= 0 && m_selectedIdx < m_filtered.size()) {
            m_commands[m_filtered[m_selectedIdx]].action();
            close();
            emit closed();
        }
    });
}

void CommandPalette::show(const QVector<Command> &commands)
{
    m_commands = commands;
    m_filtered.clear();
    for (int i = 0; i < m_commands.size(); i++)
        m_filtered.append(i);
    m_selectedIdx = 0;
    m_scrollOffset = 0;
    m_input->clear();

    // Auto-size height based on item count, capped at 400
    int itemCount = qMin(m_filtered.size(), 14);
    int newHeight = 44 + itemCount * 28 + 8;
    setFixedSize(500, qBound(200, newHeight, 400));

    if (parentWidget()) {
        QPoint center = parentWidget()->rect().center();
        int yPos = qMax(60, parentWidget()->height() / 6);
        QPoint pos = parentWidget()->mapToGlobal(QPoint(center.x() - width() / 2, yPos));
        move(pos);
    }

    QWidget::show();
    m_input->setFocus();
    raise();
}

void CommandPalette::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.setBrush(QColor(0x25, 0x25, 0x26));
    p.setPen(QPen(QColor(0x3c, 0x3c, 0x3c), 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

    // Input area separator
    p.setPen(QColor(0x3c, 0x3c, 0x3c));
    p.drawLine(0, 36, width(), 36);

    // Filtered commands
    int y = 44;
    int maxVisible = (height() - 44) / 28;
    int totalFiltered = m_filtered.size();

    if (totalFiltered == 0) {
        p.setPen(QColor(0x96, 0x96, 0x96));
        p.drawText(rect().adjusted(0, 36, 0, 0), Qt::AlignCenter,
                   QStringLiteral("无匹配命令"));
        return;
    }

    int visibleCount = qMin(totalFiltered - m_scrollOffset, maxVisible);
    for (int i = 0; i < visibleCount; i++) {
        int listIdx = m_scrollOffset + i;
        int idx = m_filtered[listIdx];
        QRect itemRect(0, y, width(), 28);
        bool selected = (listIdx == m_selectedIdx);

        if (selected)
            p.fillRect(itemRect, QColor(0x09, 0x47, 0x71));

        p.setPen(QColor(0xcc, 0xcc, 0xcc));
        p.drawText(itemRect.adjusted(14, 0, 0, 0), Qt::AlignVCenter, m_commands[idx].label);

        if (!m_commands[idx].shortcut.isEmpty()) {
            p.setPen(QColor(0x96, 0x96, 0x96));
            p.drawText(itemRect.adjusted(0, 0, -14, 0), Qt::AlignVCenter | Qt::AlignRight,
                       m_commands[idx].shortcut);
        }

        y += 28;
    }
}

void CommandPalette::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        emit closed();
        return;
    }
    if (event->key() == Qt::Key_Down) {
        if (m_selectedIdx < m_filtered.size() - 1) {
            m_selectedIdx++;
            ensureVisible();
            update();
        }
        return;
    }
    if (event->key() == Qt::Key_Up) {
        if (m_selectedIdx > 0) {
            m_selectedIdx--;
            ensureVisible();
            update();
        }
        return;
    }
    if (event->key() == Qt::Key_PageDown) {
        int maxVisible = (height() - 44) / 28;
        m_selectedIdx = qMin(m_selectedIdx + maxVisible, m_filtered.size() - 1);
        ensureVisible();
        update();
        return;
    }
    if (event->key() == Qt::Key_PageUp) {
        int maxVisible = (height() - 44) / 28;
        m_selectedIdx = qMax(m_selectedIdx - maxVisible, 0);
        ensureVisible();
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

void CommandPalette::ensureVisible()
{
    int maxVisible = (height() - 44) / 28;
    if (m_selectedIdx < m_scrollOffset)
        m_scrollOffset = m_selectedIdx;
    else if (m_selectedIdx >= m_scrollOffset + maxVisible)
        m_scrollOffset = m_selectedIdx - maxVisible + 1;
}
