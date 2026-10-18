#ifndef MISSINGFILESDIALOG_H
#define MISSINGFILESDIALOG_H

#include <QDialog>

class LibraryController;
class QLabel;
class QListWidget;
class QPushButton;

// Lists rows whose file is gone and offers to relink or remove them. Relinking
// reuses the existing row, so tags and favorites survive a moved folder.
class MissingFilesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MissingFilesDialog(LibraryController *controller, QWidget *parent = nullptr);

private:
    void reload();
    void relinkSelected();
    void removeSelected();

    LibraryController *m_controller;
    QListWidget *m_list;
    QLabel *m_summary;
    QPushButton *m_relinkButton;
    QPushButton *m_removeButton;
};

#endif
