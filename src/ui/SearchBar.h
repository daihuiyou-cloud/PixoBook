#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QAction>
#include <QLabel>
#include <QTimer>

class SearchBar : public QWidget
{
    Q_OBJECT
public:
    explicit SearchBar(QWidget *parent = nullptr);
    ~SearchBar() override = default;

    QString keyword() const;
    QString sourceFilter() const;
    QString sortField() const;
    bool sortAscending() const;
    bool onlyFavorites() const;
    void focusSearch();
    void setFavFilter(bool on);
    void clearFavFilter();
    void setThumbnailSizeSelection(int size);
    void setResultSummary(const QString &text);

signals:
    void searchRequested();
    void filterChanged();
    void thumbnailSizeChanged(int size);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateFilterSummary();

    QLineEdit *m_searchInput;
    QComboBox *m_sourceCombo;
    QComboBox *m_sortCombo;
    QPushButton *m_sizeSmallBtn;
    QPushButton *m_sizeMediumBtn;
    QPushButton *m_sizeLargeBtn;
    QButtonGroup *m_sizeGroup;
    QAction *m_clearAction;
    QPushButton *m_favButton;
    QLabel *m_resultSummary;
    QLabel *m_filterSummary;
    QPushButton *m_clearFiltersButton;
    QTimer *m_debounceTimer;
    bool m_onlyFavorites = false;
    bool m_ready = false;
};

#endif
