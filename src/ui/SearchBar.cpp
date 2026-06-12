#include "SearchBar.h"
#include <QAction>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include "ui/Codicon.h"
#include "ui/ColorConstants.h"
#include "ui/VisualConstants.h"

namespace {
QIcon iconFor(const QString &name, const QColor &color = Color::TEXT_SECONDARY, int pixelSize = 14)
{
    QPixmap px(18, 18);
    px.fill(Qt::transparent);
    QPainter p(&px);
    Codicon::draw(p, name, QRect(0, 0, 18, 18), color, pixelSize);
    return QIcon(px);
}
}

SearchBar::SearchBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(Visual::ToolbarHeight);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 10, 6);
    layout->setSpacing(8);

    QFont controlFont = font();
    controlFont.setPixelSize(Visual::FontControl);

    m_searchInput = new QLineEdit();
    m_searchInput->setMinimumHeight(Visual::ControlHeight);
    m_searchInput->setFont(controlFont);
    m_searchInput->setPlaceholderText(QStringLiteral("搜索文件名、Prompt、模型..."));
    QPalette inputPal;
    inputPal.setColor(QPalette::Base, Color::BG_INPUT);
    inputPal.setColor(QPalette::Text, Color::TEXT_PRIMARY);
    inputPal.setColor(QPalette::PlaceholderText, Color::TEXT_SECONDARY);
    m_searchInput->setPalette(inputPal);
    m_searchInput->addAction(iconFor("search", Color::TEXT_SECONDARY, 15), QLineEdit::LeadingPosition);
    m_searchInput->setTextMargins(8, 4, 8, 4);

    m_clearAction = m_searchInput->addAction(iconFor("close", Color::TEXT_SECONDARY, 13), QLineEdit::TrailingPosition);
    m_clearAction->setVisible(false);
    connect(m_searchInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_clearAction->setVisible(!text.isEmpty());
    });
    connect(m_clearAction, &QAction::triggered, this, [this]() {
        m_searchInput->clear();
        m_debounceTimer->stop();
        emit searchRequested();
    });
    layout->addWidget(m_searchInput, 1);

    QPalette comboPal;
    comboPal.setColor(QPalette::Text, Color::TEXT_PRIMARY);
    comboPal.setColor(QPalette::Base, Color::BG_DARK);
    comboPal.setColor(QPalette::Highlight, Color::HIGHLIGHT);
    comboPal.setColor(QPalette::HighlightedText, Color::TEXT_PRIMARY);

    QPalette labelPal;
    labelPal.setColor(QPalette::WindowText, Color::TEXT_SECONDARY);

    auto *sourceLabel = new QLabel(QStringLiteral("来源"));
    sourceLabel->setFont(controlFont);
    sourceLabel->setPalette(labelPal);
    layout->addWidget(sourceLabel);

    m_sourceCombo = new QComboBox();
    m_sourceCombo->setMinimumHeight(Visual::ControlHeight);
    m_sourceCombo->setMinimumWidth(120);
    m_sourceCombo->setFont(controlFont);
    m_sourceCombo->setPalette(comboPal);
    m_sourceCombo->addItem(QStringLiteral("全部来源"), "");
    m_sourceCombo->addItem("Stable Diffusion", "stable-diffusion");
    m_sourceCombo->addItem("Midjourney", "midjourney");
    m_sourceCombo->addItem("DALL-E", "dalle");
    layout->addWidget(m_sourceCombo);

    auto *sortLabel = new QLabel(QStringLiteral("排序"));
    sortLabel->setFont(controlFont);
    sortLabel->setPalette(labelPal);
    layout->addWidget(sortLabel);

    m_sortCombo = new QComboBox();
    m_sortCombo->setMinimumHeight(Visual::ControlHeight);
    m_sortCombo->setMinimumWidth(100);
    m_sortCombo->setFont(controlFont);
    m_sortCombo->setPalette(comboPal);
    m_sortCombo->addItem(QStringLiteral("时间↓"), "created_at|DESC");
    m_sortCombo->addItem(QStringLiteral("时间↑"), "created_at|ASC");
    m_sortCombo->addItem(QStringLiteral("名称 A-Z"), "file_name|ASC");
    m_sortCombo->addItem(QStringLiteral("名称 Z-A"), "file_name|DESC");
    m_sortCombo->addItem(QStringLiteral("大小↑"), "file_size|ASC");
    m_sortCombo->addItem(QStringLiteral("大小↓"), "file_size|DESC");
    layout->addWidget(m_sortCombo);

    auto *sizeLabel = new QLabel(QStringLiteral("视图"));
    sizeLabel->setFont(controlFont);
    sizeLabel->setPalette(labelPal);
    layout->addWidget(sizeLabel);

    auto *sizeSegment = new QWidget();
    auto *sizeLayout = new QHBoxLayout(sizeSegment);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(0);
    m_sizeSmallBtn = new QPushButton();
    m_sizeMediumBtn = new QPushButton();
    m_sizeLargeBtn = new QPushButton();
    QVector<QPushButton *> sizeButtons = {m_sizeSmallBtn, m_sizeMediumBtn, m_sizeLargeBtn};
    const QVector<QString> iconNames = {"zoom-in", "screen-normal", "screen-full"};
    const QVector<QString> tips = {
        QStringLiteral("小缩略图 (100px)"),
        QStringLiteral("中缩略图 (180px)"),
        QStringLiteral("大缩略图 (280px)")
    };
    for (int i = 0; i < sizeButtons.size(); i++) {
        auto *btn = sizeButtons[i];
        btn->setFixedSize(32, Visual::ControlHeight);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tips[i]);
        btn->setIcon(iconFor(iconNames[i], Color::TEXT_PRIMARY, 14));
        btn->setIconSize(QSize(16, 16));
        btn->setProperty("segmented", true);
        btn->setProperty("segmentPosition", i == 0 ? "first" : (i == sizeButtons.size() - 1 ? "last" : "middle"));
        sizeLayout->addWidget(btn);
    }
    layout->addWidget(sizeSegment);
    m_sizeMediumBtn->setChecked(true);
    auto refreshSizeIcons = [sizeButtons, iconNames]() {
        for (int i = 0; i < sizeButtons.size(); i++) {
            const QColor iconColor = sizeButtons[i]->isChecked() ? Color::TEXT_BRIGHT : Color::TEXT_PRIMARY;
            sizeButtons[i]->setIcon(iconFor(iconNames[i], iconColor, 14));
        }
    };
    refreshSizeIcons();

    m_sizeGroup = new QButtonGroup(this);
    m_sizeGroup->setExclusive(true);
    m_sizeGroup->addButton(m_sizeSmallBtn, 0);
    m_sizeGroup->addButton(m_sizeMediumBtn, 1);
    m_sizeGroup->addButton(m_sizeLargeBtn, 2);
    connect(m_sizeGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, [this](int id) {
        static const int sizes[] = {100, 180, 280};
        if (id >= 0 && id < 3) emit thumbnailSizeChanged(sizes[id]);
    });
    for (auto *btn : sizeButtons) {
        connect(btn, &QPushButton::toggled, this, refreshSizeIcons);
    }

    m_favButton = new QPushButton(iconFor("star-empty", Color::TEXT_PRIMARY, 14), QStringLiteral("收藏"));
    m_favButton->setCheckable(true);
    m_favButton->setMinimumHeight(Visual::ControlHeight);
    m_favButton->setMinimumWidth(70);
    m_favButton->setFont(controlFont);
    m_favButton->setCursor(Qt::PointingHandCursor);
    m_favButton->setProperty("toolbarButton", true);
    m_favButton->setToolTip(QStringLiteral("仅显示收藏素材"));
    layout->addWidget(m_favButton);

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(300);
    connect(m_debounceTimer, &QTimer::timeout, this, &SearchBar::searchRequested);
    connect(m_searchInput, &QLineEdit::textChanged, this, [this]() {
        m_debounceTimer->start();
    });
    connect(m_searchInput, &QLineEdit::returnPressed, this, [this]() {
        m_debounceTimer->stop();
        emit searchRequested();
    });

    QPalette bgPal;
    bgPal.setColor(QPalette::Window, Color::BG_DARK);
    setPalette(bgPal);
    setAutoFillBackground(true);

    connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { if (m_ready) emit filterChanged(); });
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { if (m_ready) emit filterChanged(); });
    connect(m_favButton, &QPushButton::toggled, this, [this](bool checked) {
        m_onlyFavorites = checked;
        m_favButton->setIcon(iconFor(checked ? "star" : "star-empty",
                                     checked ? Color::FAVORITE_ON : Color::TEXT_PRIMARY, 14));
        m_favButton->setText(QStringLiteral("收藏"));
        if (m_ready) emit filterChanged();
    });

    m_ready = true;
}

void SearchBar::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter p(this);
    p.setPen(Color::BORDER);
    p.drawLine(0, height() - 1, width(), height() - 1);
}

QString SearchBar::keyword() const { return m_searchInput->text().trimmed(); }
QString SearchBar::sourceFilter() const { return m_sourceCombo->currentData().toString(); }
QString SearchBar::sortField() const { return m_sortCombo->currentData().toString().section("|", 0, 0); }
bool SearchBar::sortAscending() const { return m_sortCombo->currentData().toString().section("|", 1, 1) == "ASC"; }
bool SearchBar::onlyFavorites() const { return m_onlyFavorites; }
void SearchBar::focusSearch() { m_searchInput->setFocus(); m_searchInput->selectAll(); }
void SearchBar::setFavFilter(bool on) { m_favButton->setChecked(on); }
void SearchBar::clearFavFilter() { m_favButton->setChecked(false); }

void SearchBar::setThumbnailSizeSelection(int size)
{
    static const int sizes[] = {100, 180, 280};
    for (int i = 0; i < 3; i++) {
        if (sizes[i] == size) {
            auto *btn = m_sizeGroup->button(i);
            if (btn) btn->setChecked(true);
            break;
        }
    }
}
