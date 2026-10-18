#ifndef MISSINGFILESDIALOG_H
#define MISSINGFILESDIALOG_H

#include <QDialog>
#include <QPoint>
#include <QVector>
#include "models/Asset.h"

class LibraryController;
class MissingListWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QWidget;

// A recovery ledger for rows whose file vanished. Amber signals "attention,
// not failure": the annotations are safe and the record can be pointed at a
// moved file without losing its id, tags or favorite.
class MissingFilesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MissingFilesDialog(LibraryController *controller, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void reload();
    void applyFilter();
    void relinkSelected();
    void relinkTo(const QString &assetId, const QString &path);
    void removeSelected();
    void updateSelectionState();
    void updateEmptyState();
    void refreshCount();
    void showNotice(const QString &text, bool error);
    void setEmptyState(bool filtered);
    static QString missingAgeText(const QDateTime &missingSince);

    LibraryController *m_controller;
    QVector<Asset> m_allMissing;
    QLineEdit *m_filter = nullptr;
    MissingListWidget *m_list = nullptr;
    QLabel *m_headerIcon = nullptr;
    QLabel *m_countBadge = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_notice = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_relinkButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QWidget *m_emptyState = nullptr;
    QLabel *m_emptyIcon = nullptr;
    QLabel *m_emptyTitle = nullptr;
    QLabel *m_emptySubtitle = nullptr;
    QTimer *m_noticeTimer = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
};

#endif
