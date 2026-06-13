#include "TagPickerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QInputDialog>
#include <QLabel>

TagPickerDialog::TagPickerDialog(const QVector<Tag> &existingTags, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("\u6dfb\u52a0\u6807\u7b7e"));
    setMinimumWidth(280);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *label = new QLabel(QStringLiteral("\u9009\u62e9\u6807\u7b7e\uff1a"));
    layout->addWidget(label);

    m_list = new QListWidget(this);
    for (const auto &t : existingTags) {
        auto *item = new QListWidgetItem(t.name);
        item->setData(Qt::UserRole, t.id);
        m_list->addItem(item);
    }
    layout->addWidget(m_list);

    auto *btnLayout = new QHBoxLayout();

    auto *newBtn = new QPushButton(QStringLiteral("+ \u65b0\u5efa\u6807\u7b7e..."), this);
    btnLayout->addWidget(newBtn);

    btnLayout->addStretch();

    auto *cancelBtn = new QPushButton(QStringLiteral("\u53d6\u6d88"), this);
    auto *okBtn = new QPushButton(QStringLiteral("\u786e\u5b9a"), this);
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(okBtn);

    layout->addLayout(btnLayout);

    connect(newBtn, &QPushButton::clicked, this, [this]() {
        bool ok;
        QString name = QInputDialog::getText(this,
            QStringLiteral("\u65b0\u5efa\u6807\u7b7e"),
            QStringLiteral("\u6807\u7b7e\u540d\u79f0\uff1a"),
            QLineEdit::Normal, QString(), &ok);
        if (ok && !name.isEmpty()) {
            m_newTagName = name;
            m_isNewTag = true;
            accept();
        }
    });

    connect(okBtn, &QPushButton::clicked, this, [this]() {
        auto items = m_list->selectedItems();
        if (items.isEmpty()) return;
        m_selectedTagId = items[0]->data(Qt::UserRole).toInt();
        m_isNewTag = false;
        accept();
    });

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (item) {
            m_selectedTagId = item->data(Qt::UserRole).toInt();
            m_isNewTag = false;
            accept();
        }
    });
}

int TagPickerDialog::pickTag(QWidget *parent, const QVector<Tag> &tags, QString *outNewName)
{
    TagPickerDialog dlg(tags, parent);
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.isNewTag()) {
            if (outNewName)
                *outNewName = dlg.newTagName();
            return -1; // signals "create new"
        }
        return dlg.m_selectedTagId;
    }
    return -2; // cancelled
}
