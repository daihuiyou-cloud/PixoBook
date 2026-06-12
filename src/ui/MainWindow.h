#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QLabel>
#include "ui/GalleryWidget.h"
#include "ui/DetailPanel.h"
#include "ui/SidebarWidget.h"
#include "ui/SearchBar.h"
#include "ui/ActivityBar.h"
#include "ui/CommandPalette.h"
#include "ui/LightboxWidget.h"
#include "ui/TitleBar.h"
#include "ui/TabBar.h"
#include "services/LibraryController.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void closeEvent(QCloseEvent *event) override;
#if defined(Q_OS_WIN)
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif

private:
    void setupUI();
    void setupTitleBar();
    void setupStatusBar();
    void setupConnections();
    void setupShortcuts();
    void onImportFolder();
    void onImportFile();
    void loadAssets();
    void saveSettings();
    void loadSettings();
    void onTagEditRequested(int tagId);
    int pickTagId(const QVector<QString> &assetIds);

    // UI
    ActivityBar *m_activityBar;
    QSplitter *m_mainSplitter;
    SidebarWidget *m_sidebar;
    QWidget *m_centerPanel;
    SearchBar *m_searchBar;
    GalleryWidget *m_gallery;
    DetailPanel *m_detailPanel;
    LightboxWidget *m_lightbox;
    CommandPalette *m_commandPalette;
    TitleBar *m_titleBar;
    TabBar *m_tabBar;
    QLabel *m_statusMsg;
    QLabel *m_statusCount;

    // Controller
    LibraryController *m_library;

    // State
    QStringList m_folders;
    int m_activeTagId = -1;
};

#endif
