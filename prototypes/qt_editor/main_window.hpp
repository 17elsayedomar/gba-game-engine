#pragma once
// =============================================================================
// MainWindow — creates every dockable panel and the menus, and connects them.
//
// QMainWindow + QDockWidget give dockable, floatable, tabbable panels with no
// extra library. QUndoStack + QUndoView give undo/redo menu actions and a
// clickable history list almost for free.
// =============================================================================

#include <QListWidget>
#include <QMainWindow>
#include <QSyntaxHighlighter>

#include <string>

class EditorContext;
class SceneTreeModel;
class SceneView;
class QPlainTextEdit;
class QTreeView;
class QTimer;

// The asset browser. Dragging an item carries its AssetId (see kAssetMimeType).
class AssetList : public QListWidget {
    Q_OBJECT
public:
    using QListWidget::QListWidget;

protected:
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;
};

// Colors C++ in the Generated Source panel. Qt calls highlightBlock() once per
// line, and only again for lines that changed.
class CppHighlighter : public QSyntaxHighlighter {
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;

protected:
    void highlightBlock(const QString& text) override;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(EditorContext* context);

    bool openScene(const QString& path);
    void selectFirstObject();
    void benchmarkStep(int step);  // --benchmark-drag: one simulated drag movement
    void useOpenGLViewport();      // --opengl: draw the Scene viewport with the GPU

private:
    void buildMenus();
    void refreshAssets();
    void refreshSource();
    void syncHierarchySelection();

    void newScene();
    void openSceneDialog();
    void saveScene(bool askForPath);
    void importPngDialog();
    void createObject();
    void deleteSelected();

    EditorContext* context_;
    SceneTreeModel* treeModel_;
    QTreeView* tree_;
    SceneView* sceneView_;
    AssetList* assets_;
    QPlainTextEdit* source_;
    QPlainTextEdit* console_;
    QTimer* sourceTimer_;
    QString scenePath_;
    int benchmarkSession_ = 0;
};
