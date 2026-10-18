#include "MissingFilesDialog.h"
#include "services/LibraryController.h"
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

MissingFilesDialog::MissingFilesDialog(LibraryController *controller, QWidget *parent)
    : QDialog(parent)
    , m_controller(controller)
{
    setWindowTitle(tr("缺失文件"));
    resize(720, 440);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    auto *hint = new QLabel(
        tr("缺失文件的标签与收藏仍保留。重新定位会复用原记录，不会新建素材。"), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    auto *buttons = new QHBoxLayout();
    m_relinkButton = new QPushButton(tr("重新定位..."), this);
    m_removeButton = new QPushButton(tr("移除记录"), this);
    auto *closeButton = new QPushButton(tr("关闭"), this);
    buttons->addWidget(m_relinkButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch();
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);

    connect(m_relinkButton, &QPushButton::clicked, this, &MissingFilesDialog::relinkSelected);
    connect(m_removeButton, &QPushButton::clicked, this, &MissingFilesDialog::removeSelected);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        relinkSelected();
    });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    reload();
}

void MissingFilesDialog::reload()
{
    m_list->clear();
    const auto missing = m_controller->getMissingAssets();
    for (const auto &asset : missing) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1    %2").arg(asset.fileName, asset.filePath));
        item->setData(Qt::UserRole, asset.id);
        item->setToolTip(asset.filePath);
        m_list->addItem(item);
    }
    const bool hasItems = !missing.isEmpty();
    m_relinkButton->setEnabled(hasItems);
    m_removeButton->setEnabled(hasItems);
    m_summary->setText(hasItems ? tr("共 %1 个缺失文件。").arg(missing.size())
                                : tr("没有缺失文件。"));
}

void MissingFilesDialog::relinkSelected()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;

    const QString assetId = item->data(Qt::UserRole).toString();
    const Asset asset = m_controller->getAsset(assetId);
    QString startDir;
    if (!asset.filePath.isEmpty()) {
        const int slash = qMax(asset.filePath.lastIndexOf(QLatin1Char('/')),
                               asset.filePath.lastIndexOf(QLatin1Char('\\')));
        if (slash > 0)
            startDir = asset.filePath.left(slash);
    }

    const QString path = QFileDialog::getOpenFileName(
        this, tr("重新定位文件"), startDir, tr("图片 (*.png *.jpg *.jpeg *.webp)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!m_controller->relinkAsset(assetId, path, &error))
        QMessageBox::warning(this, tr("重新定位失败"), error);
    reload();
}

void MissingFilesDialog::removeSelected()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;

    const QString assetId = item->data(Qt::UserRole).toString();
    const Asset asset = m_controller->getAsset(assetId);
    if (asset.id.isEmpty())
        return;

    const auto reply = QMessageBox::question(
        this, tr("移除记录"),
        tr("永久删除该记录及其标签/收藏？\n%1").arg(asset.fileName),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    (void)m_controller->deleteAssets({asset});
    reload();
}
