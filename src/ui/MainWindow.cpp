#include "MainWindow.h"
#include <QVBoxLayout>
#include <QWindow>
#include <QScreen>
#if defined(Q_OS_WIN)
#include <windows.h>
#include <windowsx.h>
#endif
#include <QStatusBar>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QShortcut>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include "ui/ToastNotification.h"
#include "ui/TagPickerDialog.h"
#include "ui/Codicon.h"
#include "database/DatabaseManager.h"
#include "services/ImageCache.h"
#include "services/FileScanner.h"
#include "services/FileWatcher.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbPath);
    dbPath += "/aimateriallibrary.db";

    // Create dependencies
    m_db = new DatabaseManager(dbPath);
    if (!m_db->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize database");
        return;
    }

    m_concreteCache = new ImageCache(500, this);
    auto *scanner = new FileScanner(this);
    auto *watcher = new FileWatcher(this);

    m_library = new LibraryController(m_db, m_concreteCache, scanner, watcher, this);

    // Frameless window
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

    setupUI();
    setupTitleBar();
    setupStatusBar();
    setupConnections();
    setupShortcuts();
    loadSettings();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (m_titleBar)
            m_titleBar->setMaximized(isMaximized());
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::setupUI()
{
    auto *central = new QWidget();
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setCentralWidget(central);

    // Title bar (replaces QMenuBar)
    m_titleBar = new TitleBar();
    mainLayout->addWidget(m_titleBar);

    // Content area
    auto *contentWidget = new QWidget();
    auto *contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_activityBar = new ActivityBar();
    contentLayout->addWidget(m_activityBar);

    m_mainSplitter = new QSplitter(Qt::Horizontal);
    m_mainSplitter->setHandleWidth(1);
    contentLayout->addWidget(m_mainSplitter, 1);

    m_sidebar = new SidebarWidget();
    m_mainSplitter->addWidget(m_sidebar);

    m_centerPanel = new QWidget();
    auto *centerLayout = new QVBoxLayout(m_centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    m_tabBar = new TabBar();
    m_searchBar = new SearchBar();
    m_gallery = new GalleryWidget(m_library->cache());

    centerLayout->addWidget(m_tabBar);
    centerLayout->addWidget(m_searchBar);
    centerLayout->addWidget(m_gallery, 1);

    m_mainSplitter->addWidget(m_centerPanel);

    m_detailPanel = new DetailPanel(m_library->cache());
    m_detailPanel->setVisible(false);
    m_mainSplitter->addWidget(m_detailPanel);

    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);
    m_mainSplitter->setSizes({220, 700, 0});

    mainLayout->addWidget(contentWidget, 1);

    m_lightbox = new LightboxWidget(m_library->cache(), this);
    m_lightbox->hide();
}

void MainWindow::setupTitleBar()
{
    connect(m_titleBar, &TitleBar::menuTriggered, this, [this](int menuIndex) {
        switch (menuIndex) {
        case 0: { // File
            QMenu menu(this);
            QMenu *importSub = menu.addMenu(QStringLiteral("\u5bfc\u5165"));
            QAction *importFolder = importSub->addAction(QStringLiteral("\u5bfc\u5165\u6587\u4ef6\u5939..."));
            connect(importFolder, &QAction::triggered, this, [this]() {
                QString dir = QFileDialog::getExistingDirectory(this,
                    QStringLiteral("\u9009\u62e9\u7d20\u6750\u6587\u4ef6\u5939"));
                if (!dir.isEmpty()) {
                    m_library->scanFolder(dir);
                    m_sidebar->setFolders(m_library->folders());
                }
            });
            QAction *importFile = importSub->addAction(QStringLiteral("\u5bfc\u5165\u56fe\u7247..."));
            connect(importFile, &QAction::triggered, this, &MainWindow::onImportFile);
            menu.addSeparator();
            QAction *exit = menu.addAction(QStringLiteral("\u9000\u51fa"));
            connect(exit, &QAction::triggered, qApp, &QApplication::quit);
            QPoint pos = m_titleBar->mapToGlobal(QPoint(m_titleBar->menuItemRect(0).left(), m_titleBar->menuItemRect(0).bottom()));
            menu.exec(pos);
            break;
        }
        case 1: { // Edit
            QMenu menu(this);
            QAction *selectAll = menu.addAction(QStringLiteral("\u5168\u9009"));
            connect(selectAll, &QAction::triggered, this, [this]() {
                QKeyEvent ev(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
                QApplication::sendEvent(m_gallery, &ev);
            });
            QPoint pos = m_titleBar->mapToGlobal(QPoint(m_titleBar->menuItemRect(1).left(), m_titleBar->menuItemRect(1).bottom()));
            menu.exec(pos);
            break;
        }
        case 2: { // View
            QMenu menu(this);
            QAction *smallThumb = menu.addAction(QStringLiteral("\u5c0f\u7f29\u7565\u56fe (100px)"));
            connect(smallThumb, &QAction::triggered, this, [this]() { m_gallery->setThumbnailSize(100); });
            QAction *mediumThumb = menu.addAction(QStringLiteral("\u4e2d\u7f29\u7565\u56fe (180px)"));
            connect(mediumThumb, &QAction::triggered, this, [this]() { m_gallery->setThumbnailSize(180); });
            QAction *largeThumb = menu.addAction(QStringLiteral("\u5927\u7f29\u7565\u56fe (280px)"));
            connect(largeThumb, &QAction::triggered, this, [this]() { m_gallery->setThumbnailSize(280); });
            QPoint pos = m_titleBar->mapToGlobal(QPoint(m_titleBar->menuItemRect(2).left(), m_titleBar->menuItemRect(2).bottom()));
            menu.exec(pos);
            break;
        }
        case 3: { // Help
            QMenu menu(this);
            QAction *about = menu.addAction(QStringLiteral("\u5173\u4e8e AI\u7d20\u6750\u5e93"));
            connect(about, &QAction::triggered, this, [this]() {
                QMessageBox::about(this, QStringLiteral("\u5173\u4e8e"),
                    QStringLiteral("AI\u7d20\u6750\u5e93 v1.0.0\nAI \u751f\u6210\u7d20\u6750\u7ba1\u7406\u5de5\u5177"));
            });
            QPoint pos = m_titleBar->mapToGlobal(QPoint(m_titleBar->menuItemRect(3).left(), m_titleBar->menuItemRect(3).bottom()));
            menu.exec(pos);
            break;
        }
        }
    });
}

void MainWindow::setupStatusBar()
{
    QPalette sbPal;
    sbPal.setColor(QPalette::Window, QColor(0x00, 0x7a, 0xcc));
    sbPal.setColor(QPalette::WindowText, Qt::white);
    statusBar()->setPalette(sbPal);
    statusBar()->setAutoFillBackground(true);

    m_statusMsg = new QLabel(QStringLiteral("\u5c31\u7eea"));
    m_statusMsg->setPalette(sbPal);
    statusBar()->addWidget(m_statusMsg, 1);

    m_statusCount = new QLabel();
    m_statusCount->setPalette(sbPal);
    statusBar()->addPermanentWidget(m_statusCount);
}

void MainWindow::setupConnections()
{
    // Library signals
    connect(m_library, &LibraryController::scanProgress, this, [this](int cur, int total) {
        m_statusMsg->setText(QStringLiteral("\u626b\u63cf\u4e2d %1/%2...").arg(cur).arg(total));
    });

    connect(m_library, &LibraryController::scanFinished, this, [this]() {
        m_statusMsg->setText(QStringLiteral("\u5c31\u7eea"));
        loadAssets();
    });

    connect(m_library, &LibraryController::dataChanged, this, [this]() {
        m_statusCount->setText(
            QStringLiteral("%1 \u5f20\u56fe\u7247").arg(m_gallery->assetCount()));
    });

    // Cache thumbnail ready -> gallery update
    connect(m_concreteCache, &ImageCache::thumbnailReady, m_gallery, &GalleryWidget::onThumbnailReady, Qt::QueuedConnection);

    // Sidebar
    connect(m_sidebar, &SidebarWidget::tagEditRequested, this, &MainWindow::onTagEditRequested);
    connect(m_sidebar, &SidebarWidget::tagDeleteRequested, this, [this](int tagId) {
        auto reply = QMessageBox::question(this,
            QStringLiteral("\u5220\u9664\u6807\u7b7e"),
            QStringLiteral("\u786e\u5b9a\u5220\u9664\u8be5\u6807\u7b7e\uff1f"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_library->deleteTag(tagId);
            if (m_activeTagId == tagId)
                m_activeTagId = -1;
            loadAssets();
        }
    });
    connect(m_sidebar, &SidebarWidget::folderSelected, this, [this](const QString &path) {
        m_library->scanFolder(path);
    });
    connect(m_sidebar, &SidebarWidget::addFolderClicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this,
            QStringLiteral("\u9009\u62e9\u7d20\u6750\u6587\u4ef6\u5939"));
        if (!dir.isEmpty()) {
            m_library->scanFolder(dir);
            m_sidebar->setFolders(m_library->folders());
        }
    });
    connect(m_sidebar, &SidebarWidget::tagSelected, this, [this](int tagId) {
        m_activeTagId = tagId;
        loadAssets();
    });
    connect(m_sidebar, &SidebarWidget::folderRefreshRequested, this, [this](const QString &path) {
        m_library->scanFolder(path);
    });
    connect(m_sidebar, &SidebarWidget::folderRemoveRequested, this, [this](const QString &path) {
        m_library->removeFolder(path);
        m_sidebar->setFolders(m_library->folders());
        m_activeTagId = -1;
        loadAssets();
    });

    // Gallery
    connect(m_gallery, &GalleryWidget::assetSelected, this, [this](const Asset &asset) {
        if (asset.id.isEmpty()) {
            m_detailPanel->clear();
            m_detailPanel->setVisible(false);
            return;
        }
        Metadata meta = m_library->getMetadata(asset.id);
        QVector<Tag> tags = m_library->getTagsForAsset(asset.id);
        m_detailPanel->showAsset(asset, meta, tags);
        m_detailPanel->setVisible(true);
        auto sizes = m_mainSplitter->sizes();
        if (sizes[2] < 300)
            m_mainSplitter->setSizes({220, sizes[1] - 300, 300});
    });

    connect(m_gallery, &GalleryWidget::assetDoubleClicked, this, [this](const Asset &asset) {
        int idx = m_gallery->selectedAssetIndex();
        if (idx >= 0)
            m_lightbox->show(m_gallery->allAssets(), idx);
    });

    connect(m_gallery, &GalleryWidget::deleteRequested, this, [this](const QVector<Asset> &assets) {
        if (assets.isEmpty()) return;
        QString msg = assets.size() == 1
            ? QStringLiteral("\u786e\u5b9a\u5220\u9664\u8be5\u9879\uff1f")
            : QStringLiteral("\u786e\u5b9a\u5220\u9664\u8fd9 %1 \u4e2a\u9879\u76ee\uff1f").arg(assets.size());
        auto reply = QMessageBox::question(this, QStringLiteral("\u5220\u9664"), msg,
                                            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_library->deleteAssets(assets);
            if (m_detailPanel->isVisible() && !assets.isEmpty()
                && m_detailPanel->currentAssetId() == assets[0].id) {
                m_detailPanel->clear();
                m_detailPanel->setVisible(false);
            }
            loadAssets();
        }
    });

    connect(m_gallery, &GalleryWidget::tagAddRequested, this, [this](const QVector<QString> &assetIds) {
        int tagId = pickTagId(assetIds);
        if (tagId >= 0) {
            m_library->addTagToAssets(assetIds, tagId);
            loadAssets();
        }
    });

    connect(m_gallery, &GalleryWidget::favoriteToggled, this, [this](const QString &assetId, bool isFavorite) {
        m_library->toggleFavorite(assetId, isFavorite);
        loadAssets();
    });

    connect(m_gallery, &GalleryWidget::filesDropped, this, [this](const QStringList &paths) {
        for (const auto &path : paths) {
            QFileInfo fi(path);
            if (fi.isDir())
                m_library->scanFolder(path);
            else if (fi.isFile() && FileScanner::isSupportedFormat(path))
                m_library->scanAndInsertFile(path);
        }
        loadAssets();
    });

    // Search bar
    connect(m_searchBar, &SearchBar::searchRequested, this, [this]() { loadAssets(); });
    connect(m_searchBar, &SearchBar::filterChanged, this, [this]() { loadAssets(); });
    connect(m_searchBar, &SearchBar::thumbnailSizeChanged, m_gallery, &GalleryWidget::setThumbnailSize);

    // Tab bar
    connect(m_tabBar, &TabBar::currentChanged, this, [this](int index) {
        Q_UNUSED(index)
        loadAssets();
    });

    // Detail panel
    connect(m_detailPanel, &DetailPanel::tagAddRequested, this, [this](const QString &assetId) {
        int tagId = pickTagId({assetId});
        if (tagId >= 0) {
            m_library->addTagToAssets({assetId}, tagId);
            Asset a = m_library->getAsset(assetId);
            if (!a.id.isEmpty()) {
                Metadata meta = m_library->getMetadata(assetId);
                QVector<Tag> tags = m_library->getTagsForAsset(assetId);
                m_detailPanel->showAsset(a, meta, tags);
            }
            loadAssets();
        }
    });

    connect(m_detailPanel, &DetailPanel::favoriteToggled, this, [this](const QString &assetId, bool isFavorite) {
        m_library->toggleFavorite(assetId, isFavorite);
        loadAssets();
    });

    connect(m_detailPanel, &DetailPanel::tagRemoved, this, [this](const QString &assetId, int tagId) {
        m_library->removeTagFromAsset(assetId, tagId);
        Asset a = m_library->getAsset(assetId);
        if (!a.id.isEmpty()) {
            Metadata meta = m_library->getMetadata(assetId);
            QVector<Tag> tags = m_library->getTagsForAsset(assetId);
            m_detailPanel->showAsset(a, meta, tags);
        }
        loadAssets();
    });

    // Lightbox
    connect(m_lightbox, &LightboxWidget::closed, this, [this]() { m_gallery->setFocus(); });
    connect(m_lightbox, &LightboxWidget::favoriteToggled, this, [this](const QString &assetId, bool isFavorite) {
        m_library->toggleFavorite(assetId, isFavorite);
        loadAssets();
    });

    // Activity bar
    connect(m_activityBar, &ActivityBar::activitySelected, this, [this](ActivityBar::Activity act) {
        m_sidebar->setVisible(true);
        switch (act) {
        case ActivityBar::Gallery:
            m_sidebar->setActiveFolder(QString());
            m_activeTagId = -1;
            m_searchBar->clearFavFilter();
            loadAssets();
            break;
        case ActivityBar::Favorites:
            m_sidebar->setActiveFolder(QString());
            m_activeTagId = -1;
            m_searchBar->setFavFilter(true);
            loadAssets();
            break;
        case ActivityBar::Tags:
            m_sidebar->setActiveTag(-1);
            m_activeTagId = -1;
            loadAssets();
            break;
        case ActivityBar::Settings:
            ToastNotification::show(this, QStringLiteral("设置功能即将推出"));
            break;
        }
    });
}

void MainWindow::setupShortcuts()
{
    auto *searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, m_searchBar, &SearchBar::focusSearch);

    m_commandPalette = new CommandPalette(this);
    auto *cpShortcut = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
    connect(cpShortcut, &QShortcut::activated, this, [this]() {
        QVector<CommandPalette::Command> cmds;
        cmds.append({QStringLiteral("\u5bfc\u5165\u6587\u4ef6\u5939..."), "Ctrl+O",
                     [this]() {
                         QString dir = QFileDialog::getExistingDirectory(this,
                             QStringLiteral("\u9009\u62e9\u7d20\u6750\u6587\u4ef6\u5939"));
                         if (!dir.isEmpty()) m_library->scanFolder(dir);
                     }});
        cmds.append({QStringLiteral("\u5bfc\u5165\u56fe\u7247..."), "Ctrl+I",
                     [this]() { MainWindow::onImportFile(); }});
        cmds.append({QStringLiteral("\u641c\u7d22..."), "Ctrl+F",
                     [this]() { m_searchBar->focusSearch(); }});
        cmds.append({QStringLiteral("\u5168\u9009"), "Ctrl+A",
                     [this]() {
                         QKeyEvent ev(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
                         QApplication::sendEvent(m_gallery, &ev);
                     }});
        cmds.append({QStringLiteral("\u4ec5\u663e\u793a\u6536\u85cf"), "",
                     [this]() { m_searchBar->setFavFilter(!m_searchBar->onlyFavorites()); }});
        m_commandPalette->show(cmds);
    });

    connect(m_commandPalette, &CommandPalette::closed, this, [this]() {
        m_gallery->setFocus();
    });
}

void MainWindow::onImportFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        QStringLiteral("\u9009\u62e9\u7d20\u6750\u6587\u4ef6\u5939"));
    if (!dir.isEmpty()) {
        m_library->scanFolder(dir);
        m_sidebar->setFolders(m_library->folders());
    }
}

void MainWindow::onImportFile()
{
    QString file = QFileDialog::getOpenFileName(this,
        QStringLiteral("\u5bfc\u5165\u56fe\u7247"),
        QString(),
        QStringLiteral("\u56fe\u7247 (*.png *.jpg *.jpeg *.webp)"));
    if (!file.isEmpty()) {
        m_library->scanAndInsertFile(file);
        loadAssets();
        m_statusMsg->setText(QStringLiteral("\u5c31\u7eea"));
        ToastNotification::show(this, QStringLiteral("\u5df2\u5bfc\u5165 1 \u5f20\u56fe\u7247"));
    }
}

void MainWindow::loadAssets()
{
    QString keyword = m_searchBar->keyword();
    QString source = m_searchBar->sourceFilter();
    bool onlyFavs = m_searchBar->onlyFavorites();

    QVector<int> tagIds;
    if (m_activeTagId >= 0)
        tagIds.append(m_activeTagId);

    auto assets = m_library->loadAssets(keyword, source, tagIds, onlyFavs,
                                        m_searchBar->sortField(), m_searchBar->sortAscending());
    m_gallery->setAssets(assets);
    m_gallery->setSearchKeyword(keyword);
    m_sidebar->setTags(m_library->getAllTags());

    m_statusCount->setText(
        QStringLiteral("%1 \u5f20\u56fe\u7247").arg(assets.size()));
}

int MainWindow::pickTagId(const QVector<QString> &assetIds)
{
    Q_UNUSED(assetIds)
    auto allTags = m_library->getAllTags();
    QString newName;
    int tagId = TagPickerDialog::pickTag(this, allTags, &newName);
    if (tagId == -1 && !newName.isEmpty()) {
        tagId = m_library->createTag(newName);
    }
    return tagId;
}

void MainWindow::onTagEditRequested(int tagId)
{
    auto allTags = m_library->getAllTags();
    Tag tag;
    for (const auto &t : allTags) {
        if (t.id == tagId) { tag = t; break; }
    }
    if (tag.id < 0) return;

    bool ok;
    QString newName = QInputDialog::getText(this,
        QStringLiteral("\u7f16\u8f91\u6807\u7b7e"),
        QStringLiteral("\u6807\u7b7e\u540d\u79f0\uff1a"),
        QLineEdit::Normal, tag.name, &ok);
    if (ok && !newName.isEmpty() && newName != tag.name) {
        m_library->renameTag(tagId, newName);
        loadAssets();
    }
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("folders", m_library->folders());
    auto sizes = m_mainSplitter->sizes();
    QVariantList sl;
    for (int s : sizes) sl.append(s);
    settings.setValue("splitterSizes", sl);

    settings.setValue("windowGeometry", saveGeometry());
}

void MainWindow::loadSettings()
{
    QSettings settings;
    QStringList folders = settings.value("folders").toStringList();
    if (!folders.isEmpty()) {
        m_library->setFolders(folders);
        m_sidebar->setFolders(folders);
        QVariant splitterSizes = settings.value("splitterSizes");
        if (splitterSizes.isValid()) {
            QList<int> sizes;
            for (const auto &v : splitterSizes.toList())
                sizes.append(v.toInt());
            m_mainSplitter->setSizes(sizes);
        }
        m_library->scanFolder(folders.first());
        for (const auto &dir : folders)
            m_library->addFolder(dir);
    }

    QByteArray geo = settings.value("windowGeometry").toByteArray();
    if (!geo.isEmpty())
        restoreGeometry(geo);
}

#if defined(Q_OS_WIN)
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType)
    MSG *msg = static_cast<MSG *>(message);

    if (msg->message == WM_NCHITTEST) {
        *result = 0;
        const LONG borderWidth = 8;
        const LONG cornerWidth = 16;
        RECT winrect;
        GetWindowRect(msg->hwnd, &winrect);
        long x = GET_X_LPARAM(msg->lParam);
        long y = GET_Y_LPARAM(msg->lParam);

        bool top = y < winrect.top + borderWidth;
        bool bottom = y > winrect.bottom - borderWidth;
        bool left = x < winrect.left + borderWidth;
        bool right = x > winrect.right - borderWidth;

        // If maximized, don't show resize edges
        if (m_titleBar && m_titleBar->isMaximized()) {
            *result = HTCLIENT;
            return true;
        }

        if (top && left)        *result = HTTOPLEFT;
        else if (top && right)  *result = HTTOPRIGHT;
        else if (bottom && left) *result = HTBOTTOMLEFT;
        else if (bottom && right) *result = HTBOTTOMRIGHT;
        else if (top)           *result = HTTOP;
        else if (bottom)        *result = HTBOTTOM;
        else if (left)          *result = HTLEFT;
        else if (right)         *result = HTRIGHT;

        if (*result != 0)
            return true;
    }

    if (msg->message == WM_GETMINMAXINFO) {
        MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);
        mmi->ptMinTrackSize.x = 800;
        mmi->ptMinTrackSize.y = 500;
        *result = 0;
        return true;
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif
