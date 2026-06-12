#include "SearchBar.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QButtonGroup>
#include <QAction>
#include <QPainter>
#include <QPaintEvent>
#include "ui/Codicon.h"
#include "ui/ColorConstants.h"

SearchBar::SearchBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);

    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText(QString::fromUtf8(
        "\xe6\x90\x9c\xe7\xb4\xa2\xe6\x96\x87\xe4\xbb\xb6\xe5\x90\x8d\xe3\x80\x81\x70\x72\x6f\x6d\x70\x74\x2e\x2e\x2e"));
    QPalette inputPal;
    inputPal.setColor(QPalette::Base, Color::BG_INPUT);
    inputPal.setColor(QPalette::Text, Color::TEXT_PRIMARY);
    m_searchInput->setPalette(inputPal);

    // Search icon
    QPixmap searchPx(22, 22);
    searchPx.fill(Qt::transparent);
    {
        QPainter sp(&searchPx);
        Codicon::draw(sp, "search", QRect(0, 0, 22, 22), Color::TEXT_SECONDARY, 16);
    }
    m_searchInput->addAction(QIcon(searchPx), QLineEdit::LeadingPosition);
    m_searchInput->setTextMargins(10, 5, 10, 5);

    // Clear button (trailing)
    QPixmap clearPx(16, 16);
    clearPx.fill(Qt::transparent);
    {
        QPainter cp(&clearPx);
        Codicon::draw(cp, "close", QRect(0, 0, 16, 16), Color::TEXT_SECONDARY, 14);
    }
    m_clearAction = m_searchInput->addAction(QIcon(clearPx), QLineEdit::TrailingPosition);
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

    m_sourceCombo = new QComboBox();
    m_sourceCombo->addItem(QString::fromUtf8("\xe5\x85\xa8\xe9\x83\xa8\xe6\x9d\xa5\xe6\xba\x90"), "");
    m_sourceCombo->addItem("Stable Diffusion", "stable-diffusion");
    m_sourceCombo->addItem("Midjourney", "midjourney");
    m_sourceCombo->addItem("DALL-E", "dalle");
    QFont comboFont;
    m_sourceCombo->setFont(comboFont);
    QPalette comboPal;
    comboPal.setColor(QPalette::Text, Color::TEXT_PRIMARY);
    comboPal.setColor(QPalette::Base, Color::BG_DARK);
    comboPal.setColor(QPalette::Highlight, Color::HIGHLIGHT);
    comboPal.setColor(QPalette::HighlightedText, Color::TEXT_PRIMARY);
    m_sourceCombo->setPalette(comboPal);
    layout->addWidget(m_sourceCombo);

    m_sortCombo = new QComboBox();
    m_sortCombo->addItem(QString::fromUtf8("\xe6\x97\xb6\xe9\x97\xb4\xe2\x86\x93"), "created_at|DESC");
    m_sortCombo->addItem(QString::fromUtf8("\xe6\x97\xb6\xe9\x97\xb4\xe2\x86\x91"), "created_at|ASC");
    m_sortCombo->addItem(QString::fromUtf8("\xe5\x90\x8d\xe7\xa7\xb0 A-Z"), "file_name|ASC");
    m_sortCombo->addItem(QString::fromUtf8("\xe5\x90\x8d\xe7\xa7\xb0 Z-A"), "file_name|DESC");
    m_sortCombo->addItem(QString::fromUtf8("\xe5\xa4\xa7\xe5\xb0\x8f\xe2\x86\x91"), "file_size|ASC");
    m_sortCombo->addItem(QString::fromUtf8("\xe5\xa4\xa7\xe5\xb0\x8f\xe2\x86\x93"), "file_size|DESC");
    m_sortCombo->setFont(comboFont);
    m_sortCombo->setPalette(comboPal);
    layout->addWidget(m_sortCombo);

    // Thumbnail size buttons
    auto *sizeLabel = new QLabel(QString::fromUtf8("\xe7\xbc\xa9\xe7\x95\xa5\xe5\x9b\xbe:"));
    QPalette labelPal;
    labelPal.setColor(QPalette::WindowText, Color::TEXT_SECONDARY);
    sizeLabel->setPalette(labelPal);
    layout->addWidget(sizeLabel);

    m_sizeSmallBtn = new QPushButton();
    m_sizeSmallBtn->setCheckable(true);
    m_sizeSmallBtn->setToolTip(QString::fromUtf8("\xe5\xb0\x8f\xe7\xbc\xa9\xe7\x95\xa5\xe5\x9b\xbe (100px)"));
    { QPixmap px(16, 16); px.fill(Qt::transparent); QPainter sp(&px); Codicon::draw(sp, "zoom-in", QRect(0, 0, 16, 16), QColor(0xcc, 0xcc, 0xcc), 14); m_sizeSmallBtn->setIcon(QIcon(px)); m_sizeSmallBtn->setIconSize(QSize(16, 16)); }
    layout->addWidget(m_sizeSmallBtn);

    m_sizeMediumBtn = new QPushButton();
    m_sizeMediumBtn->setCheckable(true);
    m_sizeMediumBtn->setChecked(true);
    m_sizeMediumBtn->setToolTip(QString::fromUtf8("\xe4\xb8\xad\xe7\xbc\xa9\xe7\x95\xa5\xe5\x9b\xbe (180px)"));
    { QPixmap px(16, 16); px.fill(Qt::transparent); QPainter sp(&px); Codicon::draw(sp, "screen-normal", QRect(0, 0, 16, 16), QColor(0xcc, 0xcc, 0xcc), 14); m_sizeMediumBtn->setIcon(QIcon(px)); m_sizeMediumBtn->setIconSize(QSize(16, 16)); }
    layout->addWidget(m_sizeMediumBtn);

    m_sizeLargeBtn = new QPushButton();
    m_sizeLargeBtn->setCheckable(true);
    m_sizeLargeBtn->setToolTip(QString::fromUtf8("\xe5\xa4\xa7\xe7\xbc\xa9\xe7\x95\xa5\xe5\x9b\xbe (280px)"));
    { QPixmap px(16, 16); px.fill(Qt::transparent); QPainter sp(&px); Codicon::draw(sp, "screen-full", QRect(0, 0, 16, 16), QColor(0xcc, 0xcc, 0xcc), 14); m_sizeLargeBtn->setIcon(QIcon(px)); m_sizeLargeBtn->setIconSize(QSize(16, 16)); }
    layout->addWidget(m_sizeLargeBtn);

    m_sizeGroup = new QButtonGroup(this);
    m_sizeGroup->addButton(m_sizeSmallBtn, 0);
    m_sizeGroup->addButton(m_sizeMediumBtn, 1);
    m_sizeGroup->addButton(m_sizeLargeBtn, 2);
    connect(m_sizeGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, [this](int id) {
        static const int sizes[] = {100, 180, 280};
        if (id >= 0 && id < 3) emit thumbnailSizeChanged(sizes[id]);
    });

    m_favButton = new QPushButton(QString::fromUtf8(" \xe6\x94\xb6\xe8\x97\x8f"));
    m_favButton->setCheckable(true);
    { QPixmap px(16, 16); px.fill(Qt::transparent); QPainter sp(&px); Codicon::draw(sp, "star-empty", QRect(0, 0, 16, 16), QColor(0xcc, 0xcc, 0xcc), 14); m_favButton->setIcon(QIcon(px)); m_favButton->setIconSize(QSize(16, 16)); }
    layout->addWidget(m_favButton);

    // Debounced real-time search
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
        QPixmap px(16, 16); px.fill(Qt::transparent); { QPainter sp(&px); Codicon::draw(sp, checked ? "star" : "star-empty", QRect(0, 0, 16, 16), QColor(0xcc, 0xcc, 0xcc), 14); }
        m_favButton->setIcon(QIcon(px)); m_favButton->setIconSize(QSize(16, 16));
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
bool SearchBar::sortAscending() const {
    return m_sortCombo->currentData().toString().section("|", 1, 1) == "ASC";
}
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
