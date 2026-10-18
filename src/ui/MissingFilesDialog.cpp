#include "MissingFilesDialog.h"
#include "ColorConstants.h"
#include "Codicon.h"
#include "services/LibraryController.h"
#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace {

enum ItemRole {
    RoleAssetId = Qt::UserRole + 1,
    RoleFavorite,
    RoleTagCount,
    RoleMissingSince,
    RoleFileName,
    RoleFilePath,
    RoleAgeText,
};

QString parentDirectory(const QString &path)
{
    const int slash = qMax(path.lastIndexOf(QLatin1Char('/')),
                           path.lastIndexOf(QLatin1Char('\\')));
    return slash > 0 ? path.left(slash) : QString();
}

} // namespace

// Drag a file from Explorer onto a row: the smallest possible relink gesture.
class MissingListWidget final : public QListWidget
{
    Q_OBJECT
public:
    explicit MissingListWidget(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        setAcceptDrops(true);
        setDragDropMode(QAbstractItemView::DropOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setUniformItemSizes(true);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFrameShape(QFrame::NoFrame);
        setMouseTracking(true);
        viewport()->setMouseTracking(true);
        viewport()->setAttribute(Qt::WA_Hover, true);
        viewport()->setAutoFillBackground(false);
        setStyleSheet(QStringLiteral(
            "QListWidget{background:transparent;border:none;outline:none;}"
            "QListWidget::item{background:transparent;border:none;}"));
    }

signals:
    void fileDropped(const QString &assetId, const QString &path);
    void relinkRequested();
    void removeRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            emit relinkRequested();
            return;
        }
        if (event->key() == Qt::Key_Delete) {
            emit removeRequested();
            return;
        }
        QListWidget::keyPressEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (!event->mimeData()->hasUrls())
            return;
        if (auto *item = itemAt(event->pos()))
            setCurrentItem(item);
        event->acceptProposedAction();
    }

    void dropEvent(QDropEvent *event) override
    {
        const auto urls = event->mimeData()->urls();
        auto *item = itemAt(event->pos());
        if (urls.isEmpty() || !item || !urls.first().isLocalFile())
            return;
        emit fileDropped(item->data(RoleAssetId).toString(), urls.first().toLocalFile());
        event->acceptProposedAction();
    }
};

class MissingItemDelegate final : public QStyledItemDelegate
{
public:
    explicit MissingItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
        const QFont base = QApplication::font();
        m_titleFont = base;
        m_titleFont.setPixelSize(13);
        m_titleFont.setWeight(QFont::DemiBold);
        m_metaFont = base;
        m_metaFont.setPixelSize(11);
        m_chipFont = base;
        m_chipFont.setPixelSize(10);
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(0, 72);
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);

        const QRect row = option.rect.adjusted(8, 3, -8, -3);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;

        if (selected || hovered) {
            p->setPen(Qt::NoPen);
            p->setBrush(selected ? Color::BG_SELECTED : Color::BG_HOVER);
            p->drawRoundedRect(row, 7, 7);
        }
        if (selected) {
            p->setBrush(Color::WARNING);
            p->drawRoundedRect(QRect(row.left() + 1, row.top() + 10, 3,
                                     row.height() - 20), 2, 2);
        }

        const QString fileName = index.data(RoleFileName).toString();
        const QString filePath = index.data(RoleFilePath).toString();
        const bool favorite = index.data(RoleFavorite).toBool();
        const int tagCount = index.data(RoleTagCount).toInt();
        const QString age = index.data(RoleAgeText).toString();

        const int thumbSize = 52;
        const QRect thumb(row.left() + 12,
                          row.top() + (row.height() - thumbSize) / 2,
                          thumbSize, thumbSize);
        p->setPen(QPen(Color::BORDER, 1));
        p->setBrush(Color::BG_PREVIEW);
        p->drawRoundedRect(thumb, 5, 5);
        Codicon::draw(*p, QStringLiteral("file-media"), thumb,
                      Color::TEXT_DISABLED, 22);
        p->setPen(QPen(Color::BG_DARK, 2));
        p->setBrush(Color::WARNING);
        p->drawEllipse(QPoint(thumb.right() - 2, thumb.top() + 2), 4, 4);

        const int rightWidth = 118;
        const int textLeft = thumb.right() + 14;
        const int textWidth = qMax(0, row.right() - rightWidth - textLeft);

        const QRect nameRect(textLeft, row.top() + 13, textWidth, 21);
        p->setFont(m_titleFont);
        p->setPen(selected ? Color::TEXT_BRIGHT : Color::TEXT_PRIMARY);
        p->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(m_titleFont).elidedText(fileName, Qt::ElideMiddle,
                                                         nameRect.width()));

        const QRect pathRect(textLeft, nameRect.bottom() + 1, textWidth, 16);
        p->setFont(m_metaFont);
        p->setPen(Color::TEXT_SECONDARY);
        p->drawText(pathRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(m_metaFont).elidedText(filePath, Qt::ElideMiddle,
                                                         pathRect.width()));

        const QRect ageRect(row.right() - rightWidth, row.top() + 13, rightWidth, 18);
        p->setFont(m_metaFont);
        p->setPen(Color::WARNING);
        p->drawText(ageRect, Qt::AlignRight | Qt::AlignVCenter, age);

        int chipRight = row.right();
        if (favorite) {
            const QRect starRect(chipRight - 15, row.bottom() - 27, 14, 16);
            Codicon::draw(*p, QStringLiteral("star"), starRect, Color::FAVORITE_ON, 13);
            chipRight -= 20;
        }
        if (tagCount > 0) {
            const QString count = QString::number(tagCount);
            const int chipWidth = 20 + QFontMetrics(m_chipFont).horizontalAdvance(count);
            const QRect chip(chipRight - chipWidth, row.bottom() - 29, chipWidth, 18);
            p->setPen(QPen(Color::BORDER_MUTED, 1));
            p->setBrush(Color::BG_CARD_SUBTLE);
            p->drawRoundedRect(chip, 4, 4);
            Codicon::draw(*p, QStringLiteral("tag"),
                          QRect(chip.left() + 3, chip.top(), 13, chip.height()),
                          Color::TEXT_SECONDARY, 11);
            p->setFont(m_chipFont);
            p->setPen(Color::TEXT_SECONDARY);
            p->drawText(chip.adjusted(17, 0, -5, 0),
                        Qt::AlignRight | Qt::AlignVCenter, count);
        }

        p->restore();
    }

private:
    QFont m_titleFont;
    QFont m_metaFont;
    QFont m_chipFont;
};

QString MissingFilesDialog::missingAgeText(const QDateTime &missingSince)
{
    if (!missingSince.isValid())
        return QString();
    const int days = missingSince.date().daysTo(QDate::currentDate());
    if (days <= 0)
        return QStringLiteral("今天");
    if (days == 1)
        return QStringLiteral("昨天");
    if (days < 7)
        return QString::number(days) + QStringLiteral(" 天前");
    if (days < 30)
        return QString::number(days / 7) + QStringLiteral(" 周前");
    if (days < 365)
        return QString::number(days / 30) + QStringLiteral(" 个月前");
    return QString::number(days / 365) + QStringLiteral(" 年前");
}

MissingFilesDialog::MissingFilesDialog(LibraryController *controller, QWidget *parent)
    : QDialog(parent)
    , m_controller(controller)
{
    setWindowTitle(tr("缺失文件"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(640, 400);
    resize(760, 480);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);

    auto *content = new QVBoxLayout();
    content->setContentsMargins(20, 16, 20, 16);
    content->setSpacing(12);
    outer->addLayout(content);

    // Header: amber tile, title/subtitle, count chip, close.
    auto *header = new QHBoxLayout();
    header->setSpacing(12);

    m_headerIcon = new QLabel(this);
    m_headerIcon->setFixedSize(38, 38);
    m_headerIcon->setAlignment(Qt::AlignCenter);
    m_headerIcon->setFont(Codicon::font(20));
    header->addWidget(m_headerIcon);

    auto *titles = new QVBoxLayout();
    titles->setSpacing(1);
    auto *title = new QLabel(tr("缺失文件"), this);
    QFont titleFont = font();
    titleFont.setPixelSize(16);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color:#ffffff;"));
    m_subtitle = new QLabel(tr("文件暂时不可达，标签与收藏都还在"), this);
    m_subtitle->setStyleSheet(QStringLiteral("color:#969696;font-size:11px;"));
    titles->addWidget(title);
    titles->addWidget(m_subtitle);
    header->addLayout(titles, 1);

    m_countBadge = new QLabel(this);
    m_countBadge->setAlignment(Qt::AlignCenter);
    m_countBadge->setFixedHeight(22);
    m_countBadge->setMinimumWidth(24);
    m_countBadge->setStyleSheet(QStringLiteral(
        "QLabel{background:rgba(215,160,74,0.16);color:#d7a04a;"
        "border:1px solid rgba(215,160,74,0.35);border-radius:11px;"
        "padding:0 8px;font-size:11px;font-weight:600;}"));
    header->addWidget(m_countBadge);

    auto *closeButton = new QPushButton(this);
    closeButton->setProperty("toolbarButton", true);
    closeButton->setIcon(Codicon::cachedIcon(QStringLiteral("close"),
                                             Color::TEXT_SECONDARY, 16));
    closeButton->setFixedSize(28, 28);
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setFocusPolicy(Qt::NoFocus);
    closeButton->setToolTip(tr("关闭"));
    header->addWidget(closeButton);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    content->addLayout(header);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("筛选文件名或路径"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setFixedHeight(30);
    m_filter->setTextMargins(10, 0, 8, 0);
    content->addWidget(m_filter);
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &) {
        applyFilter();
    });

    auto *shortcut = new QShortcut(QKeySequence::Find, this);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        m_filter->setFocus();
        m_filter->selectAll();
    });

    m_list = new MissingListWidget(this);
    m_list->setItemDelegate(new MissingItemDelegate(m_list));
    content->addWidget(m_list, 1);
    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *, QListWidgetItem *) { updateSelectionState(); });
    connect(m_list, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { relinkSelected(); });
    connect(m_list, &MissingListWidget::fileDropped, this,
            [this](const QString &assetId, const QString &path) {
                relinkTo(assetId, path);
            });
    connect(m_list, &MissingListWidget::relinkRequested,
            this, &MissingFilesDialog::relinkSelected);
    connect(m_list, &MissingListWidget::removeRequested,
            this, &MissingFilesDialog::removeSelected);

    m_emptyState = new QWidget(this);
    auto *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(0, 16, 0, 16);
    emptyLayout->setSpacing(8);
    emptyLayout->addStretch();
    m_emptyIcon = new QLabel(this);
    m_emptyIcon->setFixedSize(56, 56);
    m_emptyIcon->setAlignment(Qt::AlignCenter);
    m_emptyIcon->setFont(Codicon::font(26));
    emptyLayout->addWidget(m_emptyIcon, 0, Qt::AlignHCenter);
    m_emptyTitle = new QLabel(this);
    QFont emptyTitleFont = font();
    emptyTitleFont.setPixelSize(15);
    emptyTitleFont.setWeight(QFont::DemiBold);
    m_emptyTitle->setFont(emptyTitleFont);
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    m_emptyTitle->setStyleSheet(QStringLiteral("color:#cccccc;"));
    emptyLayout->addWidget(m_emptyTitle);
    m_emptySubtitle = new QLabel(this);
    m_emptySubtitle->setAlignment(Qt::AlignCenter);
    m_emptySubtitle->setStyleSheet(QStringLiteral("color:#969696;font-size:11px;"));
    emptyLayout->addWidget(m_emptySubtitle);
    emptyLayout->addStretch();
    content->addWidget(m_emptyState, 1);
    m_emptyState->hide();

    m_notice = new QLabel(this);
    m_notice->setWordWrap(true);
    m_notice->hide();
    content->addWidget(m_notice);

    auto *footer = new QHBoxLayout();
    footer->setSpacing(8);
    m_hint = new QLabel(tr("可直接把文件拖到对应条目上"), this);
    m_hint->setStyleSheet(QStringLiteral("color:#969696;font-size:11px;"));
    footer->addWidget(m_hint);
    footer->addStretch();

    m_removeButton = new QPushButton(tr("移除记录"), this);
    m_removeButton->setProperty("danger", true);
    m_removeButton->setFixedHeight(30);
    m_removeButton->setMinimumWidth(86);
    footer->addWidget(m_removeButton);

    m_relinkButton = new QPushButton(tr("重新定位..."), this);
    m_relinkButton->setProperty("primary", true);
    m_relinkButton->setFixedHeight(30);
    m_relinkButton->setMinimumWidth(108);
    footer->addWidget(m_relinkButton);

    auto *closeFooter = new QPushButton(tr("关闭"), this);
    closeFooter->setFixedHeight(30);
    closeFooter->setMinimumWidth(72);
    closeFooter->setFocusPolicy(Qt::NoFocus);
    footer->addWidget(closeFooter);
    content->addLayout(footer);

    connect(m_relinkButton, &QPushButton::clicked,
            this, &MissingFilesDialog::relinkSelected);
    connect(m_removeButton, &QPushButton::clicked,
            this, &MissingFilesDialog::removeSelected);
    connect(closeFooter, &QPushButton::clicked, this, &QDialog::accept);

    m_noticeTimer = new QTimer(this);
    m_noticeTimer->setSingleShot(true);
    m_noticeTimer->setInterval(3500);
    connect(m_noticeTimer, &QTimer::timeout, m_notice, &QLabel::hide);

    reload();
    resize(760, qBound(400, 170 + m_allMissing.size() * 72, 560));
    m_list->setFocus();
}

void MissingFilesDialog::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(Color::BG_DARK);
    p.setPen(QPen(Color::BORDER, 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 10, 10);
}

void MissingFilesDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() < 72) {
        m_dragging = true;
        m_dragOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void MissingFilesDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragOffset);
        event->accept();
        return;
    }
    m_dragging = false;
    QDialog::mouseMoveEvent(event);
}

void MissingFilesDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void MissingFilesDialog::reload()
{
    m_allMissing = m_controller->getMissingAssets();
    std::sort(m_allMissing.begin(), m_allMissing.end(),
              [](const Asset &a, const Asset &b) {
                  if (a.missingSince.isValid() != b.missingSince.isValid())
                      return a.missingSince.isValid();
                  return a.missingSince < b.missingSince;
              });
    applyFilter();
}

void MissingFilesDialog::applyFilter()
{
    m_list->clear();
    const QString needle = m_filter->text().trimmed();
    for (const auto &asset : qAsConst(m_allMissing)) {
        if (!needle.isEmpty()
            && !asset.fileName.contains(needle, Qt::CaseInsensitive)
            && !asset.filePath.contains(needle, Qt::CaseInsensitive))
            continue;

        auto *item = new QListWidgetItem(m_list);
        item->setData(RoleAssetId, asset.id);
        item->setData(RoleFileName, asset.fileName);
        item->setData(RoleFilePath, asset.filePath);
        item->setData(RoleFavorite, asset.isFavorite);
        item->setData(RoleTagCount, m_controller->getTagsForAsset(asset.id).size());
        item->setData(RoleMissingSince, asset.missingSince);
        item->setData(RoleAgeText, missingAgeText(asset.missingSince));
        item->setSizeHint(QSize(0, 72));
    }

    refreshCount();
    updateEmptyState();
    updateSelectionState();
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void MissingFilesDialog::refreshCount()
{
    m_countBadge->setText(QString::number(m_allMissing.size()));
    m_countBadge->setVisible(!m_allMissing.isEmpty());
    m_subtitle->setText(m_allMissing.isEmpty()
        ? tr("所有素材都在线，注释安全")
        : tr("文件暂时不可达，标签与收藏都还在"));

    if (m_allMissing.isEmpty()) {
        m_headerIcon->setText(Codicon::icon(QStringLiteral("check")));
        m_headerIcon->setStyleSheet(QStringLiteral(
            "QLabel{background:rgba(78,201,176,0.10);"
            "border:1px solid rgba(78,201,176,0.26);border-radius:9px;color:#4ec9b0;}"));
    } else {
        m_headerIcon->setText(Codicon::icon(QStringLiteral("warning")));
        m_headerIcon->setStyleSheet(QStringLiteral(
            "QLabel{background:rgba(215,160,74,0.14);"
            "border:1px solid rgba(215,160,74,0.30);border-radius:9px;color:#d7a04a;}"));
    }
}

void MissingFilesDialog::updateSelectionState()
{
    const bool hasSelection = m_list->currentItem() != nullptr;
    m_relinkButton->setEnabled(hasSelection);
    m_removeButton->setEnabled(hasSelection);
}

void MissingFilesDialog::updateEmptyState()
{
    const bool hasItems = m_list->count() > 0;
    m_list->setVisible(hasItems);
    m_emptyState->setVisible(!hasItems);
    m_hint->setVisible(!m_allMissing.isEmpty());
    m_filter->setVisible(!m_allMissing.isEmpty());
    if (hasItems)
        return;

    const bool filtered = !m_allMissing.isEmpty()
                          && !m_filter->text().trimmed().isEmpty();
    setEmptyState(filtered);
}

void MissingFilesDialog::setEmptyState(bool filtered)
{
    if (filtered) {
        m_emptyIcon->setText(Codicon::icon(QStringLiteral("search")));
        m_emptyIcon->setStyleSheet(QStringLiteral(
            "QLabel{background:rgba(215,160,74,0.12);color:#d7a04a;"
            "border:1px solid rgba(215,160,74,0.28);border-radius:28px;}"));
        m_emptyTitle->setText(tr("没有匹配的条目"));
        m_emptySubtitle->setText(tr("换个关键词，或清空筛选看看全部缺失文件。"));
    } else {
        m_emptyIcon->setText(Codicon::icon(QStringLiteral("check")));
        m_emptyIcon->setStyleSheet(QStringLiteral(
            "QLabel{background:rgba(78,201,176,0.10);color:#4ec9b0;"
            "border:1px solid rgba(78,201,176,0.26);border-radius:28px;}"));
        m_emptyTitle->setText(tr("没有缺失文件"));
        m_emptySubtitle->setText(tr("所有素材都在原来的位置。"));
    }
}

void MissingFilesDialog::relinkSelected()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;
    const QString assetId = item->data(RoleAssetId).toString();
    const QString oldPath = item->data(RoleFilePath).toString();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("重新定位文件"), parentDirectory(oldPath),
        tr("图片 (*.png *.jpg *.jpeg *.webp)"));
    if (!path.isEmpty())
        relinkTo(assetId, path);
}

void MissingFilesDialog::relinkTo(const QString &assetId, const QString &path)
{
    QString error;
    if (!m_controller->relinkAsset(assetId, path, &error)) {
        showNotice(error, true);
        return;
    }
    showNotice(tr("已重新定位，标签与收藏已保留"), false);
    reload();
}

void MissingFilesDialog::removeSelected()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;
    const Asset asset = m_controller->getAsset(item->data(RoleAssetId).toString());
    if (asset.id.isEmpty())
        return;

    const auto reply = QMessageBox::question(
        this, tr("移除记录"),
        tr("永久删除该记录及其标签/收藏？\n%1").arg(asset.fileName),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    if (!m_controller->deleteAssets({asset})) {
        showNotice(tr("移除记录失败，数据库未变更"), true);
        return;
    }
    showNotice(tr("已移除记录"), false);
    reload();
}

void MissingFilesDialog::showNotice(const QString &text, bool error)
{
    m_notice->setText(text);
    m_notice->setStyleSheet(error
        ? QStringLiteral("QLabel{background:#3c2020;color:#c06060;"
                         "border:1px solid #5a2a2a;border-radius:5px;"
                         "padding:7px 10px;font-size:11px;}")
        : QStringLiteral("QLabel{background:rgba(0,122,204,0.12);color:#7cc4f0;"
                         "border:1px solid rgba(0,122,204,0.35);border-radius:5px;"
                         "padding:7px 10px;font-size:11px;}"));
    m_notice->show();
    m_noticeTimer->start();
}

#include "MissingFilesDialog.moc"
