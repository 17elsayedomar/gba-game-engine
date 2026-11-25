#include "main_window.hpp"

#include "canvas_widgets.hpp"
#include "editor_context.hpp"
#include "inspector_panel.hpp"
#include "scene_tree_model.hpp"
#include "scene_view.hpp"

#include "proto_core/assets.hpp"
#include "proto_core/codegen.hpp"
#include "proto_core/serialization.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDockWidget>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMimeData>
#include <QOpenGLWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QTreeView>
#include <QUndoView>
#include <QVBoxLayout>

#include <cmath>

using namespace proto;

// =============================================================================
// Small panel helpers
// =============================================================================

QStringList AssetList::mimeTypes() const { return {kAssetMimeType}; }

QMimeData* AssetList::mimeData(const QList<QListWidgetItem*>& items) const {
    if (items.isEmpty()) return nullptr;
    auto* mime = new QMimeData;
    mime->setData(kAssetMimeType, QByteArray::number(items.first()->data(Qt::UserRole).toLongLong()));
    return mime;
}

void CppHighlighter::highlightBlock(const QString& text) {
    static const QRegularExpression keyword(R"(\b(static|const|extern|nullptr|int|struct|return)\b)");
    static const QRegularExpression number(R"(\b\d+\b)");
    static const QRegularExpression string(R"("[^"]*")");
    auto paint = [&](const QRegularExpression& pattern, QColor color) {
        for (auto it = pattern.globalMatch(text); it.hasNext();) {
            auto match = it.next();
            setFormat(static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength()), color);
        }
    };
    QString trimmed = text.trimmed();
    if (trimmed.startsWith('#')) return setFormat(0, static_cast<int>(text.size()), QColor(200, 120, 220));
    paint(keyword, QColor(110, 160, 255));
    paint(number, QColor(230, 170, 90));
    paint(string, QColor(220, 140, 120));
    qsizetype comment = text.indexOf("//");  // last, so comments win over everything else
    if (comment >= 0) setFormat(static_cast<int>(comment), static_cast<int>(text.size() - comment), QColor(110, 190, 110));
}

static QDockWidget* makeDock(QMainWindow* window, const QString& title, QWidget* content, Qt::DockWidgetArea area) {
    auto* dock = new QDockWidget(title, window);
    dock->setObjectName(title);  // needed if the layout is ever saved with saveState()
    dock->setWidget(content);
    window->addDockWidget(area, dock);
    return dock;
}

// =============================================================================
// MainWindow
// =============================================================================

MainWindow::MainWindow(EditorContext* context) : context_(context) {
    setWindowTitle("GBA Game Engine - Qt 6 Widgets prototype");
    setDockNestingEnabled(true);
    setDockOptions(AnimatedDocks | AllowTabbedDocks | AllowNestedDocks);

    // --- Hierarchy: QTreeView + our model.
    auto* hierarchy = new QWidget;
    auto* hierarchyLayout = new QVBoxLayout(hierarchy);
    auto* buttons = new QHBoxLayout;
    auto* createButton = new QPushButton("+ Create");
    auto* deleteButton = new QPushButton("Delete");
    buttons->addWidget(createButton);
    buttons->addWidget(deleteButton);
    buttons->addStretch();
    hierarchyLayout->addLayout(buttons);
    treeModel_ = new SceneTreeModel(context_, this);
    tree_ = new QTreeView;
    tree_->setModel(treeModel_);
    tree_->setHeaderHidden(true);
    tree_->setRootIsDecorated(false);
    tree_->setDragDropMode(QAbstractItemView::DropOnly);
    tree_->setDropIndicatorShown(true);
    tree_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    hierarchyLayout->addWidget(tree_);
    connect(createButton, &QPushButton::clicked, this, &MainWindow::createObject);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    connect(tree_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current) {
                // An invalid index also arrives while the model resets; that is not a user click.
                if (current.isValid()) context_->select(treeModel_->idAt(current));
            });
    // After a model reset Qt creates a fresh selection model state; re-select.
    connect(treeModel_, &QAbstractItemModel::modelReset, this, &MainWindow::syncHierarchySelection);

    // --- Assets.
    auto* assetsPanel = new QWidget;
    auto* assetsLayout = new QVBoxLayout(assetsPanel);
    auto* importButton = new QPushButton("Import PNG...");
    assetsLayout->addWidget(importButton, 0, Qt::AlignLeft);
    auto* assetHint = new QLabel("Drag an asset onto an object or into the Scene.");
    assetHint->setEnabled(false);
    assetsLayout->addWidget(assetHint);
    assets_ = new AssetList;
    assets_->setDragEnabled(true);
    assets_->setIconSize(QSize(48, 48));
    assetsLayout->addWidget(assets_);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importPngDialog);

    // --- Scene viewport with its small toolbar.
    auto* scenePanel = new QWidget;
    auto* sceneLayout = new QVBoxLayout(scenePanel);
    auto* sceneBar = new QHBoxLayout;
    auto* snap = new QCheckBox("Snap to 8 px grid");
    auto* zoom = new QSlider(Qt::Horizontal);
    zoom->setRange(1, 6);
    zoom->setValue(2);
    zoom->setFixedWidth(120);
    auto* zoomLabel = new QLabel("Zoom 2x");
    sceneBar->addWidget(snap);
    sceneBar->addWidget(zoom);
    sceneBar->addWidget(zoomLabel);
    sceneBar->addStretch();
    sceneLayout->addLayout(sceneBar);
    sceneView_ = new SceneView(context_);
    sceneLayout->addWidget(sceneView_);
    connect(snap, &QCheckBox::toggled, sceneView_, &SceneView::setSnapToGrid);
    connect(zoom, &QSlider::valueChanged, this, [this, zoomLabel](int value) {
        sceneView_->setZoom(value);
        zoomLabel->setText(QString("Zoom %1x").arg(value));
    });

    // --- Center: Scene / Isometric Grid / Node Graph as tabs.
    auto* center = new QTabWidget;
    center->addTab(scenePanel, "Scene");
    center->addTab(new IsoGridWidget(context_), "Isometric Grid");
    center->addTab(new NodeGraphWidget(context_), "Node Graph");
    setCentralWidget(center);

    // --- Generated source.
    auto* sourcePanel = new QWidget;
    auto* sourceLayout = new QVBoxLayout(sourcePanel);
    auto* sourceBar = new QHBoxLayout;
    auto* copyButton = new QPushButton("Copy to clipboard");
    auto* sourceLabel = new QLabel("read-only preview, regenerated on every change");
    sourceLabel->setEnabled(false);
    sourceBar->addWidget(copyButton);
    sourceBar->addWidget(sourceLabel);
    sourceBar->addStretch();
    sourceLayout->addLayout(sourceBar);
    source_ = new QPlainTextEdit;
    source_->setReadOnly(true);
    source_->setLineWrapMode(QPlainTextEdit::NoWrap);
    source_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    new CppHighlighter(source_->document());
    sourceLayout->addWidget(source_);
    connect(copyButton, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(source_->toPlainText()); });

    // --- Console.
    console_ = new QPlainTextEdit;
    console_->setReadOnly(true);
    connect(context_, &EditorContext::logMessage, console_, &QPlainTextEdit::appendPlainText);

    // --- Docks.
    QDockWidget* hierarchyDock = makeDock(this, "Hierarchy", hierarchy, Qt::LeftDockWidgetArea);
    QDockWidget* assetsDock = makeDock(this, "Assets", assetsPanel, Qt::LeftDockWidgetArea);
    splitDockWidget(hierarchyDock, assetsDock, Qt::Vertical);
    QDockWidget* inspectorDock = makeDock(this, "Inspector", new InspectorPanel(context_), Qt::RightDockWidgetArea);
    QDockWidget* sourceDock = makeDock(this, "Generated Source", sourcePanel, Qt::BottomDockWidgetArea);
    QDockWidget* consoleDock = makeDock(this, "Console", console_, Qt::BottomDockWidgetArea);
    tabifyDockWidget(sourceDock, consoleDock);
    sourceDock->raise();
    auto* undoView = new QUndoView(context_->undoStack());  // a clickable undo history, free from Qt
    undoView->setEmptyLabel("<start>");
    QDockWidget* historyDock = makeDock(this, "Undo History", undoView, Qt::RightDockWidgetArea);
    splitDockWidget(inspectorDock, historyDock, Qt::Vertical);
    // Dock sizes only stick once the window has its final size, so apply them
    // when the event loop starts (after showMaximized()).
    QTimer::singleShot(0, this, [=, this] {
        resizeDocks({hierarchyDock, inspectorDock}, {260, 360}, Qt::Horizontal);
        resizeDocks({sourceDock, historyDock}, {200, 160}, Qt::Vertical);
    });

    buildMenus();

    // --- Keeping the panels in sync with the scene (the retained-mode part).
    // Replacing the whole source document re-lays-out and re-highlights every
    // line, which is expensive during a drag (60 changes per second). So the
    // preview refreshes at most 10 times per second, and only while visible.
    // Set GBE_SOURCE_REFRESH_MS=0 to refresh on every change, for comparison.
    sourceTimer_ = new QTimer(this);
    sourceTimer_->setSingleShot(true);
    sourceTimer_->setInterval(qEnvironmentVariableIsSet("GBE_SOURCE_REFRESH_MS")
                                  ? qEnvironmentVariableIntValue("GBE_SOURCE_REFRESH_MS")
                                  : 100);
    connect(sourceTimer_, &QTimer::timeout, this, &MainWindow::refreshSource);
    connect(sourceDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) refreshSource();
    });
    connect(context_, &EditorContext::sceneChanged, this, [this] {
        treeModel_->refresh();
        refreshAssets();
        if (!sourceTimer_->isActive()) sourceTimer_->start();
    });
    connect(context_, &EditorContext::selectionChanged, this, &MainWindow::syncHierarchySelection);

    refreshAssets();
    refreshSource();
    context_->log("Qt prototype started. Import a PNG from the Assets panel to begin.");
}

void MainWindow::buildMenus() {
    QMenu* file = menuBar()->addMenu("&File");
    file->addAction("New Scene", this, &MainWindow::newScene);
    file->addAction("Open Scene...", QKeySequence::Open, this, &MainWindow::openSceneDialog);
    file->addAction("Save Scene", QKeySequence::Save, this, [this] { saveScene(false); });
    file->addAction("Save Scene As...", this, [this] { saveScene(true); });
    file->addSeparator();
    file->addAction("Import PNG...", this, &MainWindow::importPngDialog);
    file->addSeparator();
    file->addAction("Exit", this, &QWidget::close);

    QMenu* edit = menuBar()->addMenu("&Edit");
    // QUndoStack makes Undo/Redo actions that enable/disable themselves and show
    // "Undo Move Player" automatically.
    QAction* undo = context_->undoStack()->createUndoAction(this, "Undo");
    undo->setShortcuts({QKeySequence::Undo});
    QAction* redo = context_->undoStack()->createRedoAction(this, "Redo");
    redo->setShortcuts({QKeySequence("Ctrl+Y"), QKeySequence("Ctrl+Shift+Z")});
    edit->addAction(undo);
    edit->addAction(redo);
    edit->addSeparator();
    edit->addAction("Create GameObject", QKeySequence::New, this, &MainWindow::createObject);
    QAction* del = edit->addAction("Delete", this, &MainWindow::deleteSelected);
    del->setShortcut(QKeySequence::Delete);
    // Only when the Hierarchy or Scene has focus, so Delete still works in text fields.
    del->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    tree_->addAction(del);
    sceneView_->addAction(del);

    QMenu* view = menuBar()->addMenu("&View");
    for (QDockWidget* dock : findChildren<QDockWidget*>()) view->addAction(dock->toggleViewAction());
}

// =============================================================================
// Refreshing panels from the model
// =============================================================================

void MainWindow::refreshAssets() {
    const Scene& scene = context_->scene();
    if (assets_->count() == static_cast<int>(scene.assets.size())) return;  // assets are only ever added
    assets_->clear();
    for (const Asset& asset : scene.assets) {
        QString text = QString("%1\n%2x%3, %4 GBA colors")
                           .arg(QString::fromStdString(asset.name))
                           .arg(asset.original.width)
                           .arg(asset.original.height)
                           .arg(asset.gbaColorCount);
        for (const std::string& warning : asset.warnings) text += "\n" + QString::fromStdString(warning);
        QPixmap thumb = context_->pixmapFor(asset.id).scaled(48, 48, Qt::KeepAspectRatio, Qt::FastTransformation);
        auto* item = new QListWidgetItem(QIcon(thumb), text);
        item->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(asset.id));
        assets_->addItem(item);
    }
}

void MainWindow::refreshSource() {
    if (!source_->isVisible()) return;  // refreshed when its tab is shown again
    QString text = QString::fromStdString(generateSceneSource(context_->scene()));
    if (text == source_->toPlainText()) return;
    int scroll = source_->verticalScrollBar()->value();
    source_->setPlainText(text);
    source_->verticalScrollBar()->setValue(scroll);  // keep the reader's place
}

void MainWindow::syncHierarchySelection() {
    QModelIndex index = treeModel_->indexOf(context_->selected());
    QSignalBlocker block(tree_->selectionModel());  // don't echo back to context_->select()
    if (index.isValid())
        tree_->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    else
        tree_->selectionModel()->clear();
    tree_->viewport()->update();
}

// =============================================================================
// Actions
// =============================================================================

void MainWindow::newScene() {
    context_->replaceScene(makeDefaultScene());
    scenePath_.clear();
    context_->log("New scene.");
}

bool MainWindow::openScene(const QString& path) {
    std::string error;
    auto loaded = loadSceneFile(path.toStdString(), error);
    if (!loaded) {
        context_->log("Open failed: " + QString::fromStdString(error));
        return false;
    }
    context_->replaceScene(std::move(*loaded));
    scenePath_ = path;
    context_->log("Opened " + path);
    return true;
}

void MainWindow::openSceneDialog() {
    QString path = QFileDialog::getOpenFileName(this, "Open Scene", {}, "Scene files (*.json)");
    if (!path.isEmpty()) openScene(path);
}

void MainWindow::saveScene(bool askForPath) {
    QString path = scenePath_;
    if (askForPath || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save Scene", {}, "Scene files (*.json)");
        if (path.isEmpty()) return;
    }
    std::string error;
    if (saveSceneFile(context_->scene(), path.toStdString(), error)) {
        scenePath_ = path;
        context_->log("Saved " + path);
    } else {
        context_->log("Save failed: " + QString::fromStdString(error));
    }
}

void MainWindow::importPngDialog() {
    QString path = QFileDialog::getOpenFileName(this, "Import PNG", {}, "PNG images (*.png)");
    if (path.isEmpty()) return;
    std::string error;
    auto asset = importPng(path.toStdString(), error);
    if (!asset) {
        context_->log("Import failed: " + QString::fromStdString(error));
        return;
    }
    asset->id = context_->scene().allocateId();
    context_->log(QString("Imported %1 (%2 GBA colors)").arg(QString::fromStdString(asset->name)).arg(asset->gbaColorCount));
    for (const std::string& warning : asset->warnings) context_->log("  Warning: " + QString::fromStdString(warning));
    context_->scene().assets.push_back(std::move(*asset));
    context_->notifyChanged();
}

void MainWindow::createObject() {
    auto command = std::make_unique<CreateObjectCommand>(
        "GameObject " + std::to_string(context_->scene().objects.size() + 1), 112, 72);
    CreateObjectCommand* raw = command.get();
    context_->execute(std::move(command));
    context_->select(raw->createdId());
}

void MainWindow::deleteSelected() {
    if (context_->selected() != 0) context_->execute(std::make_unique<DeleteObjectCommand>(context_->selected()));
}

void MainWindow::useOpenGLViewport() {
    // QGraphicsView normally paints with the CPU. Giving it a QOpenGLWidget as
    // its "viewport" makes the same items render through OpenGL instead.
    sceneView_->setViewport(new QOpenGLWidget);
}

void MainWindow::selectFirstObject() {
    if (!context_->scene().objects.empty()) context_->select(context_->scene().objects.front().id);
}

void MainWindow::benchmarkStep(int step) {
    const GameObject* object = context_->scene().findObject(context_->selected());
    if (!object) return;
    if (benchmarkSession_ == 0) benchmarkSession_ = context_->newSession();
    double t = step * 0.05;
    int x = 112 + static_cast<int>(std::lround(80 * std::cos(t)));
    int y = 72 + static_cast<int>(std::lround(50 * std::sin(t)));
    context_->execute(std::make_unique<SetPositionCommand>(object->id, x, y, object->z, benchmarkSession_));
}
