#include "MainWindow.h"
#include <QVBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QInputDialog>
#include <QShortcut>
#include <QMap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbPath);
    dbPath += "/aimateriallibrary.db";

    m_db = new DatabaseManager(dbPath);
    if (!m_db->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize database");
        return;
    }

    m_cache = new ImageCache(500, this);
    m_scanner = new FileScanner(this);
    m_watcher = new FileWatcher(this);

    setupUI();
    setupMenuBar();
    setupStatusBar();

    connect(m_scanner, &FileScanner::assetFound, this, [this](const Asset &asset) {
        Asset existing = m_db->findByPath(asset.filePath);
        if (existing.id.isEmpty() && m_db->findByHash(asset.hash).id.isEmpty()) {
            m_db->insertAsset(asset);
            Metadata meta = MetadataParser::parse(asset.filePath);
            if (!meta.source.isEmpty()) {
                meta.assetId = asset.id;
                m_db->upsertMetadata(meta);
            }
        }
    });

    connect(m_scanner, &FileScanner::scanProgress, this, [this](int cur, int total) {
        statusBar()->showMessage(
            QString::fromUtf8("\xe6\x89\xab\xe6\x8f\x8f\xe4\xb8\xad %1/%2...").arg(cur).arg(total));
    });

    connect(m_scanner, &FileScanner::scanFinished, this, [this]() {
        loadAssets();
        statusBar()->showMessage(
            QString::fromUtf8("\xe5\xb7\xb2\xe5\x8a\xa0\xe8\xbd\xbd %1 \xe5\xbc\xa0\xe5\x9b\xbe\xe7\x89\x87")
                .arg(m_gallery->assetCount()), 5000);
    });

    // FileWatcher signals
    connect(m_watcher, &FileWatcher::fileAdded, this, [this](const QString &path) {
        if (FileScanner::isSupportedFormat(path)) {
            scanAndInsertFile(path);
            loadAssets();
        }
    });
    connect(m_watcher, &FileWatcher::fileRemoved, this, [this](const QString &path) {
        Asset a = m_db->findByPath(path);
        if (!a.id.isEmpty()) {
            m_db->deleteAsset(a.id);
            loadAssets();
        }
    });

    // Drag-drop on gallery
    connect(m_gallery, &GalleryWidget::filesDropped, this, &MainWindow::onFilesDropped);

    // Load persisted state
    loadSettings();
}

MainWindow::~MainWindow()
{
    delete m_db;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupUI()
{
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_mainSplitter);

    m_sidebar = new SidebarWidget();
    m_mainSplitter->addWidget(m_sidebar);

    m_centerPanel = new QWidget();
    auto *centerLayout = new QVBoxLayout(m_centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    m_searchBar = new SearchBar();
    m_gallery = new GalleryWidget(m_cache);
    centerLayout->addWidget(m_searchBar);
    centerLayout->addWidget(m_gallery, 1);

    m_mainSplitter->addWidget(m_centerPanel);

    m_detailPanel = new DetailPanel(m_cache);
    m_detailPanel->setVisible(false);
    m_mainSplitter->addWidget(m_detailPanel);

    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);
    m_mainSplitter->setSizes({220, 700, 0});

    connect(m_sidebar, &SidebarWidget::tagEditRequested, this, &MainWindow::onTagEditRequested);
    connect(m_sidebar, &SidebarWidget::tagDeleteRequested, this, [this](int tagId) {
        auto reply = QMessageBox::question(this,
            QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4\xe6\xa0\x87\xe7\xad\xbe"),
            QString::fromUtf8("\xe7\xa1\xae\xe5\xae\x9a\xe5\x88\xa0\xe9\x99\xa4\xe8\xaf\xa5\xe6\xa0\x87\xe7\xad\xbe\xef\xbc\x9f"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_db->deleteTag(tagId);
            if (m_activeTagId == tagId)
                m_activeTagId = -1;
            loadAssets();
        }
    });
    connect(m_sidebar, &SidebarWidget::folderSelected, this, &MainWindow::onFolderSelected);
    connect(m_sidebar, &SidebarWidget::addFolderClicked, this, &MainWindow::onAddFolderClicked);
    connect(m_sidebar, &SidebarWidget::tagSelected, this, &MainWindow::onTagSelected);
    connect(m_sidebar, &SidebarWidget::folderRefreshRequested, this, [this](const QString &path) {
        m_watcher->addWatchPath(path);
        m_gallery->clearAssets();
        m_scanner->scanDirectory(path, true);
    });
    connect(m_sidebar, &SidebarWidget::folderRemoveRequested, this, [this](const QString &path) {
        m_folders.removeAll(path);
        m_sidebar->setFolders(m_folders);
        m_activeTagId = -1;
        loadAssets();
    });
    connect(m_gallery, &GalleryWidget::assetSelected, this, &MainWindow::onAssetSelected);
    connect(m_gallery, &GalleryWidget::assetDoubleClicked, this, &MainWindow::onAssetDoubleClicked);
    connect(m_gallery, &GalleryWidget::deleteRequested, this, &MainWindow::onDeleteRequested);
    connect(m_gallery, &GalleryWidget::tagAddRequested, this, [this](const QVector<QString> &assetIds) {
        auto allTags = m_db->getAllTags();
        QStringList tagNames;
        QMap<QString, int> nameToId;
        for (const auto &t : allTags) {
            tagNames << t.name;
            nameToId[t.name] = t.id;
        }
        tagNames << QString::fromUtf8("+ \xe6\x96\xb0\xe5\xbb\xba\xe6\xa0\x87\xe7\xad\xbe...");

        bool ok;
        QString selected = QInputDialog::getItem(this,
            QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe6\xa0\x87\xe7\xad\xbe"),
            QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe6\xa0\x87\xe7\xad\xbe\xef\xbc\x9a"),
            tagNames, 0, false, &ok);
        if (!ok || selected.isEmpty()) return;

        if (selected == QString::fromUtf8("+ \xe6\x96\xb0\xe5\xbb\xba\xe6\xa0\x87\xe7\xad\xbe...")) {
            // Create new tag
            QString newName = QInputDialog::getText(this,
                QString::fromUtf8("\xe6\x96\xb0\xe5\xbb\xba\xe6\xa0\x87\xe7\xad\xbe"),
                QString::fromUtf8("\xe6\xa0\x87\xe7\xad\xbe\xe5\x90\x8d\xe7\xa7\xb0\xef\xbc\x9a"),
                QLineEdit::Normal, QString(), &ok);
            if (!ok || newName.isEmpty()) return;

            Tag newTag;
            newTag.name = newName;
            newTag.color = QColor(0x60, 0xa0, 0xff);
            int newId = m_db->insertTag(newTag);
            if (newId < 0) return;
            for (const auto &aid : assetIds)
                m_db->addTagToAsset(aid, newId);
        } else {
            int tagId = nameToId.value(selected, -1);
            if (tagId >= 0) {
                for (const auto &aid : assetIds)
                    m_db->addTagToAsset(aid, tagId);
            }
        }
        loadAssets();
    });
    connect(m_gallery, &GalleryWidget::favoriteToggled, this, [this](const QString &assetId, bool isFavorite) {
        m_db->updateAssetFavorite(assetId, isFavorite);
        loadAssets();
    });
    connect(m_searchBar, &SearchBar::searchRequested, this, &MainWindow::onSearchRequested);
    connect(m_searchBar, &SearchBar::filterChanged, this, &MainWindow::onFilterChanged);
    connect(m_searchBar, &SearchBar::thumbnailSizeChanged, m_gallery, &GalleryWidget::setThumbnailSize);

    // Lightbox — overlay on top of everything
    m_lightbox = new LightboxWidget(m_cache, this);
    m_lightbox->lower();

    connect(m_gallery, &GalleryWidget::assetDoubleClicked, this, [this](const Asset &asset) {
        int idx = m_gallery->selectedAssetIndex();
        if (idx < 0) return;
        m_lightbox->show(m_gallery->allAssets(), idx);
    });
    connect(m_lightbox, &LightboxWidget::closed, this, [this]() {
        m_gallery->setFocus();
    });
    connect(m_lightbox, &LightboxWidget::favoriteToggled, this, [this](const QString &assetId, bool isFavorite) {
        m_db->updateAssetFavorite(assetId, isFavorite);
        loadAssets();
    });

    // Ctrl+F to focus search
    auto *searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        m_searchBar->focusSearch();
    });

    connect(m_detailPanel, &DetailPanel::tagAddRequested, this, [this](const QString &assetId) {
        auto allTags = m_db->getAllTags();
        QStringList tagNames;
        QMap<QString, int> nameToId;
        for (const auto &t : allTags) {
            tagNames << t.name;
            nameToId[t.name] = t.id;
        }
        tagNames << QString::fromUtf8("+ \xe6\x96\xb0\xe5\xbb\xba\xe6\xa0\x87\xe7\xad\xbe...");

        bool ok;
        QString selected = QInputDialog::getItem(this,
            QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe6\xa0\x87\xe7\xad\xbe"),
            QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe6\xa0\x87\xe7\xad\xbe\xef\xbc\x9a"),
            tagNames, 0, false, &ok);
        if (!ok || selected.isEmpty()) return;

        int tagId;
        if (selected == QString::fromUtf8("+ \xe6\x96\xb0\xe5\xbb\xba\xe6\xa0\x87\xe7\xad\xbe...")) {
            QString newName = QInputDialog::getText(this,
                QString::fromUtf8("\xe6\x96\xb0\xe5\xbb\xba\xe6\xa0\x87\xe7\xad\xbe"),
                QString::fromUtf8("\xe6\xa0\x87\xe7\xad\xbe\xe5\x90\x8d\xe7\xa7\xb0\xef\xbc\x9a"),
                QLineEdit::Normal, QString(), &ok);
            if (!ok || newName.isEmpty()) return;
            Tag newTag;
            newTag.name = newName;
            newTag.color = QColor(0x60, 0xa0, 0xff);
            tagId = m_db->insertTag(newTag);
            if (tagId < 0) return;
        } else {
            tagId = nameToId.value(selected, -1);
            if (tagId < 0) return;
        }
        m_db->addTagToAsset(assetId, tagId);
        // Refresh detail panel
        Asset a = m_db->getAsset(assetId);
        if (!a.id.isEmpty()) {
            Metadata meta = m_db->getMetadata(assetId);
            QVector<Tag> tags = m_db->getTagsForAsset(assetId);
            m_detailPanel->showAsset(a, meta, tags);
        }
        loadAssets();
    });
    connect(m_detailPanel, &DetailPanel::favoriteToggled, this, [this](const QString &assetId, bool isFavorite) {
        m_db->updateAssetFavorite(assetId, isFavorite);
        loadAssets();
    });
    connect(m_detailPanel, &DetailPanel::tagRemoved, this, [this](const QString &assetId, int tagId) {
        m_db->removeTagFromAsset(assetId, tagId);
        // Refresh detail panel
        Asset a = m_db->getAsset(assetId);
        if (!a.id.isEmpty()) {
            Metadata meta = m_db->getMetadata(assetId);
            QVector<Tag> tags = m_db->getTagsForAsset(assetId);
            m_detailPanel->showAsset(a, meta, tags);
        }
        loadAssets();
    });
}

void MainWindow::setupMenuBar()
{
    auto *menuBar = new QMenuBar();
    menuBar->setStyleSheet(
        "QMenuBar { background: #323233; color: #cccccc; font-size: 12px; padding: 2px; }"
        "QMenuBar::item:selected { background: #094771; }"
        "QMenu { background: #252526; color: #cccccc; border: 1px solid #3c3c3c; font-size: 12px; }"
        "QMenu::item:selected { background: #094771; }"
        "QMenu::separator { background: #3c3c3c; height: 1px; margin: 4px 8px; }"
    );

    auto *fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction(QString::fromUtf8("\xe5\xaf\xbc\xe5\x85\xa5\xe6\x96\x87\xe4\xbb\xb6\xe5\xa4\xb9..."),
                        this, &MainWindow::onImportFolder, QKeySequence::Open);
    fileMenu->addAction(QString::fromUtf8("\xe5\xaf\xbc\xe5\x85\xa5\xe5\x9b\xbe\xe7\x89\x87..."),
                        this, &MainWindow::onImportFile, QKeySequence("Ctrl+I"));
    fileMenu->addSeparator();
    fileMenu->addAction(QString::fromUtf8("\xe9\x80\x80\xe5\x87\xba"),
                        qApp, &QApplication::quit, QKeySequence::Quit);

    setMenuBar(menuBar);
}

void MainWindow::setupStatusBar()
{
    statusBar()->setStyleSheet(
        "QStatusBar { background: #007acc; color: white; font-size: 12px; padding: 2px 8px; }"
    );
    statusBar()->showMessage(QString::fromUtf8("\xe5\xb0\xb1\xe7\xbb\xaa"));
}

void MainWindow::loadAssets()
{
    QString keyword = m_searchBar->keyword();
    QString source = m_searchBar->sourceFilter();
    bool onlyFavs = m_searchBar->onlyFavorites();

    QVector<int> tagIds;
    if (m_activeTagId >= 0)
        tagIds.append(m_activeTagId);

    auto assets = m_db->searchAssets(keyword, source, tagIds, onlyFavs,
                                      m_searchBar->sortField(), m_searchBar->sortAscending());
    m_gallery->setAssets(assets);

    // Update sidebar tags
    m_sidebar->setTags(m_db->getAllTags());
}

void MainWindow::addFolderToLibrary(const QString &dir)
{
    if (!m_folders.contains(dir)) {
        m_folders.append(dir);
        m_sidebar->setFolders(m_folders);
    }
    m_watcher->addWatchPath(dir);
    m_gallery->clearAssets();
    m_scanner->scanDirectory(dir, true);
}

void MainWindow::scanAndInsertFile(const QString &path)
{
    Asset asset = FileScanner::scanSingleFile(path);
    if (asset.id.isEmpty()) return;
    if (!m_db->findByHash(asset.hash).id.isEmpty()) return;

    m_db->insertAsset(asset);
    Metadata meta = MetadataParser::parse(asset.filePath);
    if (!meta.source.isEmpty()) {
        meta.assetId = asset.id;
        m_db->upsertMetadata(meta);
    }
}

void MainWindow::onImportFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe7\xb4\xa0\xe6\x9d\x90\xe6\x96\x87\xe4\xbb\xb6\xe5\xa4\xb9"));
    if (!dir.isEmpty())
        addFolderToLibrary(dir);
}

void MainWindow::onImportFile()
{
    QString file = QFileDialog::getOpenFileName(this,
        QString::fromUtf8("\xe5\xaf\xbc\xe5\x85\xa5\xe5\x9b\xbe\xe7\x89\x87"),
        QString(),
        QString::fromUtf8("\xe5\x9b\xbe\xe7\x89\x87 (*.png *.jpg *.jpeg *.webp)"));
    if (!file.isEmpty()) {
        scanAndInsertFile(file);
        loadAssets();
        statusBar()->showMessage(
            QString::fromUtf8("\xe5\xb7\xb2\xe5\xaf\xbc\xe5\x85\xa5 1 \xe5\xbc\xa0\xe5\x9b\xbe\xe7\x89\x87"), 3000);
    }
}

void MainWindow::onFilesDropped(const QStringList &paths)
{
    for (const auto &path : paths) {
        QFileInfo fi(path);
        if (fi.isDir()) {
            addFolderToLibrary(path);
        } else if (fi.isFile() && FileScanner::isSupportedFormat(path)) {
            scanAndInsertFile(path);
        }
    }
    loadAssets();
}

void MainWindow::onFolderSelected(const QString &path)
{
    addFolderToLibrary(path);
}

void MainWindow::onAddFolderClicked()
{
    onImportFolder();
}

void MainWindow::onTagSelected(int tagId)
{
    m_activeTagId = tagId;
    loadAssets();
}

void MainWindow::onAssetSelected(const Asset &asset)
{
    if (asset.id.isEmpty()) {
        m_detailPanel->clear();
        m_detailPanel->setVisible(false);
        return;
    }

    Metadata meta = m_db->getMetadata(asset.id);
    QVector<Tag> tags = m_db->getTagsForAsset(asset.id);
    m_detailPanel->showAsset(asset, meta, tags);
    m_detailPanel->setVisible(true);

    auto sizes = m_mainSplitter->sizes();
    if (sizes[2] < 300)
        m_mainSplitter->setSizes({220, sizes[1] - 300, 300});
}

void MainWindow::onAssetDoubleClicked(const Asset &asset)
{
    // Toggle detail panel visibility
    if (m_detailPanel->isVisible() && !m_detailPanel->currentAssetId().isEmpty()
        && m_detailPanel->currentAssetId() == asset.id) {
        m_detailPanel->setVisible(false);
        auto sizes = m_mainSplitter->sizes();
        m_mainSplitter->setSizes({sizes[0], sizes[1] + sizes[2], 0});
    } else {
        Metadata meta = m_db->getMetadata(asset.id);
        QVector<Tag> tags = m_db->getTagsForAsset(asset.id);
        m_detailPanel->showAsset(asset, meta, tags);
        m_detailPanel->setVisible(true);
        auto sizes = m_mainSplitter->sizes();
        if (sizes[2] < 300)
            m_mainSplitter->setSizes({220, sizes[1] - 300, 300});
    }
}

void MainWindow::onSearchRequested()
{
    loadAssets();
}

void MainWindow::onFilterChanged()
{
    loadAssets();
}

void MainWindow::onDeleteRequested(const QVector<Asset> &assets)
{
    if (assets.isEmpty()) return;
    QString msg = assets.size() == 1
        ? QString::fromUtf8("\xe7\xa1\xae\xe5\xae\x9a\xe5\x88\xa0\xe9\x99\xa4\xe8\xaf\xa5\xe9\xa1\xb9\xef\xbc\x9f")
        : QString::fromUtf8("\xe7\xa1\xae\xe5\xae\x9a\xe5\x88\xa0\xe9\x99\xa4\xe8\xbf\x99 %1 \xe4\xb8\xaa\xe9\xa1\xb9\xe7\x9b\xae\xef\xbc\x9f").arg(assets.size());
    auto reply = QMessageBox::question(this,
        QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4"),
        msg, QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        for (const auto &a : assets)
            m_db->deleteAsset(a.id);
        if (m_detailPanel->isVisible() && !assets.isEmpty()
            && m_detailPanel->currentAssetId() == assets[0].id) {
            m_detailPanel->clear();
            m_detailPanel->setVisible(false);
        }
        loadAssets();
    }
}

void MainWindow::onTagEditRequested(int tagId)
{
    Tag tag = m_db->getAllTags().value(tagId < 0 ? -1 : -1);
    // Find tag by id
    auto allTags = m_db->getAllTags();
    for (const auto &t : allTags) {
        if (t.id == tagId) { tag = t; break; }
    }
    if (tag.id < 0) return;

    bool ok;
    QString newName = QInputDialog::getText(this,
        QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91\xe6\xa0\x87\xe7\xad\xbe"),
        QString::fromUtf8("\xe6\xa0\x87\xe7\xad\xbe\xe5\x90\x8d\xe7\xa7\xb0\xef\xbc\x9a"),
        QLineEdit::Normal, tag.name, &ok);
    if (ok && !newName.isEmpty() && newName != tag.name) {
        tag.name = newName;
        m_db->updateTag(tag);
        loadAssets();
    }
}

void MainWindow::saveSettings()
{
    QSettings settings("AIMaterialLibrary", "AIMaterialLibrary");
    settings.setValue("folders", m_folders);
    auto sizes = m_mainSplitter->sizes();
    QVariantList sl;
    for (int s : sizes) sl.append(s);
    settings.setValue("splitterSizes", sl);
}

void MainWindow::loadSettings()
{
    QSettings settings("AIMaterialLibrary", "AIMaterialLibrary");
    m_folders = settings.value("folders").toStringList();
    if (!m_folders.isEmpty()) {
        m_sidebar->setFolders(m_folders);
        QVariant splitterSizes = settings.value("splitterSizes");
        if (splitterSizes.isValid()) {
            QList<int> sizes;
            for (const auto &v : splitterSizes.toList())
                sizes.append(v.toInt());
            m_mainSplitter->setSizes(sizes);
        }
        // Auto-scan first folder
        m_gallery->clearAssets();
        m_scanner->scanDirectory(m_folders.first(), true);
        for (const auto &dir : m_folders)
            m_watcher->addWatchPath(dir);
    }
}
