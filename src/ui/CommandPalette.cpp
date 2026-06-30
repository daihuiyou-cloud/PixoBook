#include "CommandPalette.h"
#include "ColorConstants.h"
#include <QPainter>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <algorithm>
#include <QtMath>

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
    m_input->setPlaceholderText(QStringLiteral("\u8F93\u5165\u547D\u4EE4\u540D..."));
    m_input->setFixedHeight(36);
    QPalette inputPal;
    inputPal.setColor(QPalette::Base, Color::BG_INPUT);
    inputPal.setColor(QPalette::Text, Color::TEXT_PRIMARY);
    m_input->setPalette(inputPal);
    m_input->setTextMargins(12, 0, 12, 0);
    layout->addWidget(m_input);

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(100);
    connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingText != m_input->text()) {
            m_pendingText = m_input->text();
            rebuildFilter();
        }
    });

    connect(m_input, &QLineEdit::textChanged, this, [this]() {
        m_debounceTimer->start();
    });

    connect(m_input, &QLineEdit::returnPressed, this, [this]() {
        if (m_selectedIdx >= 0 && m_selectedIdx < m_filtered.size()) {
            m_commands[m_filtered[m_selectedIdx]].action();
            close();
            emit closed();
        }
    });
}

void CommandPalette::rebuildFilter()
{
    QString query = m_input->text().toLower();
    m_filtered.clear();
    m_matchPositions.clear();

    if (query.isEmpty()) {
        m_matchPositions.reserve(m_commands.size());
        for (int i = 0; i < m_commands.size(); i++) {
            m_filtered.append(i);
            m_matchPositions.append(QVector<int>());
        }
    } else {
        struct Scored {
            int index;
            int score;
            QVector<int> positions;
        };
        QVector<Scored> scored;
        scored.reserve(m_commands.size());

        for (int i = 0; i < m_commands.size(); i++) {
            QString label = m_commands[i].label.toLower();
            QVector<int> positions;
            int qi = 0;
            for (int li = 0; li < label.size() && qi < query.size(); li++) {
                if (label[li] == query[qi]) {
                    positions.append(li);
                    qi++;
                }
            }
            if (qi == query.size()) {
                int score = 0;
                for (int p : positions)
                    score += p;
                if (!positions.isEmpty()) {
                    bool consecutive = (positions.last() - positions.first() + 1 == positions.size());
                    if (consecutive)
                        score -= 100;
                    if (label.startsWith(query))
                        score -= 200;
                }
                scored.append({i, score, positions});
            }
        }

        std::sort(scored.begin(), scored.end(), [](const Scored &a, const Scored &b) {
            return a.score < b.score;
        });

        m_filtered.reserve(scored.size());
        m_matchPositions.reserve(scored.size());
        for (const auto &s : scored) {
            m_filtered.append(s.index);
            m_matchPositions.append(s.positions);
        }
    }

    m_selectedIdx = m_filtered.isEmpty() ? -1 : 0;
    m_scrollOffset = 0;

    int itemCount = qMin(m_filtered.size(), 14);
    int newHeight = 44 + qMax(1, itemCount) * 28 + 8;
    setFixedSize(500, qBound(200, newHeight, 400));

    update();
}

void CommandPalette::show(const QVector<Command> &commands)
{
    m_commands = commands;
    m_input->clear();
    m_pendingText.clear();
    rebuildFilter();

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

void CommandPalette::drawHighlightedText(QPainter &p, const QRect &rect, const QString &text,
                                         const QVector<int> &positions) const
{
    if (positions.isEmpty() || m_input->text().isEmpty()) {
        p.drawText(rect, Qt::AlignVCenter, text);
        return;
    }

    QFontMetrics fm = p.fontMetrics();
    int x = rect.left();
    int y = rect.center().y() + fm.ascent() / 2;

    QVector<QPair<int, bool>> segments;
    int pi = 0;
    for (int i = 0; i < text.size(); i++) {
        bool highlighted = (pi < positions.size() && positions[pi] == i);
        if (highlighted)
            pi++;
        if (segments.isEmpty() || segments.last().second != highlighted)
            segments.append({i, highlighted});
    }
    segments.append({text.size(), false});

    for (int s = 0; s < segments.size() - 1; ++s) {
        int start = segments[s].first;
        int end = segments[s + 1].first;
        QString piece = text.mid(start, end - start);
        if (segments[s].second) {
            p.setPen(Color::ACCENT);
            p.drawText(x, y, piece);
        } else {
            p.setPen(Color::TEXT_PRIMARY);
            p.drawText(x, y, piece);
        }
        x += fm.horizontalAdvance(piece);
    }
}

void CommandPalette::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.setBrush(Color::BG_DARK);
    p.setPen(QPen(Color::BORDER, 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

    p.setPen(Color::BORDER);
    p.drawLine(0, 36, width(), 36);

    int y = 44;
    int maxVisible = (height() - 44) / 28;
    int totalFiltered = m_filtered.size();

    if (totalFiltered == 0) {
        p.setPen(Color::TEXT_SECONDARY);
        p.drawText(rect().adjusted(0, 36, 0, -8), Qt::AlignCenter,
                   tr("无匹配命令"));
        return;
    }

    int visibleCount = qMin(totalFiltered - m_scrollOffset, maxVisible);
    p.setFont(font());
    for (int i = 0; i < visibleCount; i++) {
        int listIdx = m_scrollOffset + i;
        int idx = m_filtered[listIdx];
        QRect itemRect(0, y, width(), 28);
        bool selected = (listIdx == m_selectedIdx);

        if (selected)
            p.fillRect(itemRect, Color::HIGHLIGHT);

        QRect textRect = itemRect.adjusted(14, 0, 0, 0);
        drawHighlightedText(p, textRect, m_commands[idx].label,
                            m_matchPositions.isEmpty() ? QVector<int>() : m_matchPositions[listIdx]);

        if (!m_commands[idx].shortcut.isEmpty()) {
            p.setPen(Color::TEXT_SECONDARY);
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
