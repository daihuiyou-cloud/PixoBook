#ifndef TAGPICKERDIALOG_H
#define TAGPICKERDIALOG_H

#include <QDialog>
#include <QVector>
#include "models/Tag.h"

class QListWidget;

class TagPickerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TagPickerDialog(const QVector<Tag> &existingTags, QWidget *parent = nullptr);

    int selectedTagId() const { return m_selectedTagId; }
    QString newTagName() const { return m_newTagName; }
    bool isNewTag() const { return m_isNewTag; }

    static int pickTag(QWidget *parent, const QVector<Tag> &tags, QString *outNewName = nullptr);

private:
    QListWidget *m_list;
    int m_selectedTagId = -1;
    QString m_newTagName;
    bool m_isNewTag = false;
};

#endif
