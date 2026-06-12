#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QButtonGroup>
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

signals:
    void searchRequested();
    void filterChanged();
    void thumbnailSizeChanged(int size);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLineEdit *m_searchInput;
    QComboBox *m_sourceCombo;
    QComboBox *m_sortCombo;
    QPushButton *m_sizeSmallBtn;
    QPushButton *m_sizeMediumBtn;
    QPushButton *m_sizeLargeBtn;
    QButtonGroup *m_sizeGroup;
    QPushButton *m_favButton;
    QTimer *m_debounceTimer;
    bool m_onlyFavorites = false;
    bool m_ready = false;
};

#endif
