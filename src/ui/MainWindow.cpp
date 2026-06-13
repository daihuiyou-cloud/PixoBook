#include "MainWindow.h"
#include <QVBoxLayout>
#include <QWindow>
#include <QScreen>
#include <QGuiApplication>
#if defined(Q_OS_WIN)
#include <windows.h>
#include <windowsx.h>
#endif
#include <QStatusBar>
#include <QApplication>
#include <QFont>
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
#include "ui/ColorConstants.h"
#include "ui/VisualConstants.h"
#include "ui/UIUtils.h"
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
        QMessageBox::critical(this, tr("错误"), tr("数据库初始化失败"));
        return;
    }

    m_concreteCache = new ImageCache(256LL * 1024 * 1024, this);
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
    m_mainSplitter->setSizes({Visual::SidebarWidth, 1008, 0});

    mainLayout->addWidget(contentWidget, 1);

    m_lightbox = new LightboxWidget(this);
    m_lightbox->hide();
}

void MainWindow::setupTitleBar()
{
    connect(m_titleBar, &TitleBar::menuTriggered, this, [this](int menuIndex) {
        switch (menuIndex) {
        case 0: { // File
            QMenu menu(this);
            QMenu *importSub = menu.addMenu(tr("导入"));
            QAction *importFolder = importSub->addAction(tr("导入文件夹..."));
            connect(importFolder, &QAction::triggered, this, [this]() {
                QString dir = QFileDialog::getExistingDirectory(this,
                    tr("选择素材文件夹"));
                if (!dir.isEmpty()) {
                    m_library->scanFolder(dir);
                    m_sidebar->setFolders(m_library->folders());
                }
            });
            QAction *importFile = importSub->addAction(tr("导入图片..."));
            connect(importFile, &QAction::triggered, this, &MainWindow::onImportFile);
            menu.addSeparator();
            QAction *exit = menu.addAction(tr("退出"));
            connect(exit, &QAction::triggered, qApp, &QApplication::quit);
            QPoint pos = m_titleBar->mapToGlobal(QPoint(m_titleBar->menuItemRect(0).left(), m_titleBar->menuItemRect(0).bottom()));
            menu.exec(pos);
            break;
        }
        case 1: { // Edit
            QMenu menu(this);
            QAction *selectAll = menu.addAction(tr("全选"));
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
            int curSize = m_gallery->thumbnailSize();
            QAction *smallThumb = menu.addAction(tr("小缩略图 (100px)"));
            smallThumb->setCheckable(true); smallThumb->setChecked(curSize <= 100);
            connect(smallThumb, &QAction::triggered, this, [this]() { m_gallery->setThumbnailSize(100); m_searchBar->setThumbnailSizeSelection(100); });
            QAction *mediumThumb = menu.addAction(tr("中缩略图 (180px)"));
            mediumThumb->setCheckable(true); mediumThumb->setChecked(curSize > 100 && curSize <= 200);
            connect(mediumThumb, &QAction::triggered, this, [this]() { m_gallery->setThumbnailSize(180); m_searchBar->setThumbnailSizeSelection(180); });
            QAction *largeThumb = menu.addAction(tr("大缩略图 (280px)"));
            largeThumb->setCheckable(true); largeThumb->setChecked(curSize >= 280);
            connect(largeThumb, &QAction::triggered, this, [this]() { m_gallery->setThumbnailSize(280); m_searchBar->setThumbnailSizeSelection(280); });
            QPoint pos = m_titleBar->mapToGlobal(QPoint(m_titleBar->menuItemRect(2).left(), m_titleBar->menuItemRect(2).bottom()));
            menu.exec(pos);
            break;
        }
        case 3: { // Help
            QMenu menu(this);
            QAction *about = menu.addAction(tr("关于 AI 素材库"));
            connect(about, &QAction::triggered, this, [this]() {
                QMessageBox::about(this, tr("关于"),
                    tr("AI 素材库 v1.0.0\nAI 生成素材管理工具"));
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
    sbPal.setColor(QPalette::Window, Color::ACCENT);
    sbPal.setColor(QPalette::WindowText, Qt::white);
    statusBar()->setPalette(sbPal);
    statusBar()->setAutoFillBackground(true);

    m_statusMsg = new QLabel(tr("就绪"));
    m_statusMsg->setPalette(sbPal);
    QFont sf = m_statusMsg->font();
    sf.setPixelSize(Visual::FontControl);
    m_statusMsg->setFont(sf);
    statusBar()->addWidget(m_statusMsg, 1);

    m_statusCount = new QLabel();
    m_statusCount->setPalette(sbPal);
    m_statusCount->setFont(sf);
    statusBar()->addPermanentWidget(m_statusCount);
}

void MainWindow::setupConnections()
{
    // Library signals
    connect(m_library, &LibraryController::scanProgress, this, [this](int cur, int total) {
        m_statusMsg->setText(tr("扫描中 %1/%2...").arg(cur).arg(total));
    });

    connect(m_library, &LibraryController::scanFinished, this, [this]() {
        m_statusMsg->setText(tr("就绪"));
        loadAssets();
    });

    connect(m_library, &LibraryController::dataChanged, this, [this]() {
        m_statusCount->setText(
            tr("%1 张图片").arg(m_gallery->assetCount()));
    });

    // Cache thumbnail ready -> gallery update
    connect(m_concreteCache, &ImageCache::thumbnailReady, m_gallery, &GalleryWidget::onThumbnailReady, Qt::QueuedConnection);

    // Sidebar
    connect(m_sidebar, &SidebarWidget::tagEditRequested, this, &MainWindow::onTagEditRequested);
    connect(m_sidebar, &SidebarWidget::tagCreateRequested, this, [this]() {
        bool ok = false;
        QString name = QInputDialog::getText(this,
            tr("添加标签"),
            tr("标签名称："),
            QLineEdit::Normal, QString(), &ok);
        if (ok && !name.trimmed().isEmpty()) {
            m_library->createTag(name.trimmed());
            loadAssets();
        }
    });
    connect(m_sidebar, &SidebarWidget::tagDeleteRequested, this, [this](int tagId) {
        auto reply = QMessageBox::question(this,
            tr("删除标签"),
            tr("确定删除该标签？"),
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
            tr("选择素材文件夹"));
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
        if (sizes[2] < Visual::DetailPanelWidth)
            m_mainSplitter->setSizes({Visual::SidebarWidth,
                                      qMax(480, sizes[1] - Visual::DetailPanelWidth),
                                      Visual::DetailPanelWidth});
    });

    connect(m_gallery, &GalleryWidget::assetDoubleClicked, this, [this](const Asset &asset) {
        int idx = m_gallery->selectedAssetIndex();
        if (idx >= 0)
            m_lightbox->show(m_gallery->allAssets(), idx);
    });

    connect(m_gallery, &GalleryWidget::deleteRequested, this, [this](const QVector<Asset> &assets) {
        if (assets.isEmpty()) return;
        QString msg = assets.size() == 1
            ? tr("确定删除该项目？")
            : tr("确定删除这 %1 个项目？").arg(assets.size());
        auto reply = QMessageBox::question(this, tr("删除"), msg,
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
    connect(m_gallery, &GalleryWidget::importFolderRequested, this, &MainWindow::onImportFolder);
    connect(m_gallery, &GalleryWidget::importFilesRequested, this, &MainWindow::onImportFile);

    // Gallery lazy pagination
    connect(m_gallery, &GalleryWidget::loadMoreRequested, this, [this](int offset, int limit) {
        QString keyword = m_searchBar->keyword();
        QString source = m_searchBar->sourceFilter();
        bool onlyFavs = m_searchBar->onlyFavorites();
        QString sortField = m_searchBar->sortField();
        bool sortAsc = m_searchBar->sortAscending();
        int tabIdx = m_tabBar->currentIndex();
        if (tabIdx == 1) onlyFavs = true;
        QVector<int> tagIds;
        if (m_activeTagId >= 0) tagIds.append(m_activeTagId);
        auto assets = m_library->loadAssets(keyword, source, tagIds, onlyFavs,
                                            sortField, sortAsc, offset, limit);
        m_gallery->appendPage(assets, m_gallery->totalAssetCount());
    });

    // Search bar
    connect(m_searchBar, &SearchBar::searchRequested, this, [this]() {
        // Clear tab override when user manually searches
        m_tabBar->setCurrentIndex(0);
        loadAssets();
    });
    connect(m_searchBar, &SearchBar::filterChanged, this, [this]() { loadAssets(); });
    connect(m_searchBar, &SearchBar::thumbnailSizeChanged, m_gallery, &GalleryWidget::setThumbnailSize);

    // Tab bar
    connect(m_tabBar, &TabBar::currentChanged, this, [this](int idx) {
        m_searchBar->blockSignals(true);
        m_searchBar->setFavFilter(idx == 1);
        m_searchBar->blockSignals(false);
        loadAssets();
    });
    connect(m_tabBar, &TabBar::addTabRequested, this, [this]() {
        onImportFolder();
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

    connect(m_detailPanel, &DetailPanel::previewRequested, this, [this](const QString &assetId) {
        const auto assets = m_gallery->allAssets();
        for (int i = 0; i < assets.size(); i++) {
            if (assets[i].id == assetId) {
                m_lightbox->show(assets, i);
                break;
            }
        }
    });

    connect(m_detailPanel, &DetailPanel::promptEditRequested, this, [this](const QString &assetId) {
        Asset asset = m_library->getAsset(assetId);
        if (asset.id.isEmpty()) return;

        Metadata meta = m_library->getMetadata(assetId);
        bool ok = false;
        QString updated = QInputDialog::getMultiLineText(
            this,
            tr("编辑 Prompt"),
            tr("Prompt"),
            meta.prompt,
            &ok
        );
        if (!ok) return;

        meta.assetId = assetId;
        meta.prompt = updated.trimmed();
        if (m_library->updateMetadata(meta)) {
            QVector<Tag> tags = m_library->getTagsForAsset(assetId);
            m_detailPanel->showAsset(asset, meta, tags);
            loadAssets();
            ToastNotification::show(this, tr("已保存 Prompt"));
        }
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

        // Block signals to avoid cascading refreshes
        m_tabBar->blockSignals(true);
        m_searchBar->blockSignals(true);

        switch (act) {
        case ActivityBar::Gallery:
            m_sidebar->setActiveFolder(QString());
            m_activeTagId = -1;
            m_tabBar->setCurrentIndex(0);
            m_searchBar->clearFavFilter();
            break;
        case ActivityBar::Favorites:
            m_sidebar->setActiveFolder(QString());
            m_activeTagId = -1;
            m_tabBar->setCurrentIndex(1);
            m_searchBar->setFavFilter(true);
            break;
        case ActivityBar::Tags:
            m_sidebar->setActiveTag(-1);
            m_activeTagId = -1;
            m_tabBar->setCurrentIndex(0);
            break;
        }

        m_tabBar->blockSignals(false);
        m_searchBar->blockSignals(false);
        loadAssets();
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
        cmds.append({tr("导入文件夹..."), "Ctrl+O",
                     [this]() {
                         QString dir = QFileDialog::getExistingDirectory(this,
                             tr("选择素材文件夹"));
                         if (!dir.isEmpty()) m_library->scanFolder(dir);
                     }});
        cmds.append({tr("导入图片..."), "Ctrl+I",
                     [this]() { MainWindow::onImportFile(); }});
        cmds.append({tr("搜索..."), "Ctrl+F",
                     [this]() { m_searchBar->focusSearch(); }});
        cmds.append({tr("全选"), "Ctrl+A",
                     [this]() {
                         QKeyEvent ev(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
                         QApplication::sendEvent(m_gallery, &ev);
                     }});
        cmds.append({tr("仅显示收藏"), "",
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
        tr("选择素材文件夹"));
    if (!dir.isEmpty()) {
        m_library->scanFolder(dir);
        m_sidebar->setFolders(m_library->folders());
    }
}

void MainWindow::onImportFile()
{
    QString file = QFileDialog::getOpenFileName(this,
        tr("导入图片"),
        QString(),
        tr("图片 (*.png *.jpg *.jpeg *.webp)"));
    if (!file.isEmpty()) {
        m_library->scanAndInsertFile(file);
        loadAssets();
        m_statusMsg->setText(tr("就绪"));
        ToastNotification::show(this, tr("已导入 1 张图片"));
    }
}

void MainWindow::loadAssets()
{
    QString keyword = m_searchBar->keyword();
    QString source = m_searchBar->sourceFilter();
    bool onlyFavs = m_searchBar->onlyFavorites();
    QString sortField = m_searchBar->sortField();
    bool sortAsc = m_searchBar->sortAscending();

    // Apply tab-driven filters on top of search bar state
    int tabIdx = m_tabBar->currentIndex();
    if (tabIdx == 1) {
        onlyFavs = true;
    } else if (tabIdx == 2) {
        sortField = QStringLiteral("created_at");
        sortAsc = false;
    }

    QVector<int> tagIds;
    if (m_activeTagId >= 0)
        tagIds.append(m_activeTagId);

    int totalCount = m_library->countAssets(keyword, source, tagIds, onlyFavs);
    auto assets = m_library->loadAssets(keyword, source, tagIds, onlyFavs,
                                        sortField, sortAsc, 0, 200);
    m_gallery->setAssets(assets, totalCount);
    m_gallery->setSearchKeyword(keyword);
    m_sidebar->setTags(m_library->getAllTags());

    m_statusCount->setText(
        tr("%1 张图片").arg(totalCount));

    QStringList summary;
    summary << tr("%1 张图片").arg(totalCount);
    if (tabIdx == 1)
        summary << tr("收藏夹");
    else if (tabIdx == 2)
        summary << tr("最近导入");
    else
        summary << tr("所有素材");

    if (!source.isEmpty())
        summary << UIUtils::displayNameForSource(source);
    if (onlyFavs)
        summary << tr("仅收藏");
    if (m_activeTagId >= 0)
        summary << tr("标签筛选");
    m_searchBar->setResultSummary(summary.join(QStringLiteral(" / ")));
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
        tr("编辑标签"),
        tr("标签名称："),
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
    settings.setValue("thumbnailSize", m_gallery->thumbnailSize());
    settings.setValue("activeTab", m_tabBar->currentIndex());
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
        for (int i = 1; i < folders.size(); i++)
            m_library->addFolder(folders[i]);
    }

    // Restore thumbnail size
    int thumbSize = settings.value("thumbnailSize", 180).toInt();
    m_gallery->setThumbnailSize(thumbSize);
    m_searchBar->setThumbnailSizeSelection(thumbSize);

    // Restore active tab
    int tabIdx = settings.value("activeTab", 0).toInt();
    m_tabBar->setCurrentIndex(tabIdx);

    QByteArray geo = settings.value("windowGeometry").toByteArray();
    if (!geo.isEmpty()) {
        restoreGeometry(geo);
        // Ensure window is visible on an available screen
        QRect geom = geometry();
        bool onScreen = false;
        for (const auto *screen : QGuiApplication::screens()) {
            if (screen->availableGeometry().intersects(geom)) {
                onScreen = true;
                break;
            }
        }
        if (!onScreen) {
            move(QGuiApplication::primaryScreen()->availableGeometry().topLeft());
        }
    }
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
        mmi->ptMinTrackSize.x = 1000;
        mmi->ptMinTrackSize.y = 700;
        *result = 0;
        return true;
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif
