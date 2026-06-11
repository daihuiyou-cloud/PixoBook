#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include "ui/GalleryWidget.h"
#include "ui/DetailPanel.h"
#include "ui/SidebarWidget.h"
#include "ui/SearchBar.h"
#include "database/DatabaseManager.h"
#include "services/ImageCache.h"
#include "services/FileScanner.h"
#include "services/FileWatcher.h"
#include "parsers/MetadataParser.h"
#include "models/Tag.h"
#include "models/Asset.h"
#include "models/Metadata.h"
#include "ui/LightboxWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void closeEvent(QCloseEvent *event) override;

private slots:
    void onFolderSelected(const QString &path);
    void onAddFolderClicked();
    void onImportFolder();
    void onImportFile();
    void onFilesDropped(const QStringList &paths);
    void onAssetSelected(const Asset &asset);
    void onAssetDoubleClicked(const Asset &asset);
    void onSearchRequested();
    void onFilterChanged();
    void onTagSelected(int tagId);
    void onDeleteRequested(const QVector<Asset> &assets);
    void onTagEditRequested(int tagId);

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void loadAssets();
    void addFolderToLibrary(const QString &dir);
    void scanAndInsertFile(const QString &path);
    void saveSettings();
    void loadSettings();

    QSplitter *m_mainSplitter;
    SidebarWidget *m_sidebar;
    QWidget *m_centerPanel;
    SearchBar *m_searchBar;
    GalleryWidget *m_gallery;
    DetailPanel *m_detailPanel;

    DatabaseManager *m_db;
    ImageCache *m_cache;
    FileScanner *m_scanner;
    FileWatcher *m_watcher;

    QStringList m_folders;
    int m_activeTagId = -1;
    LightboxWidget *m_lightbox;
};

#endif
