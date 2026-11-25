#include "editor_context.hpp"

#include <QImage>

EditorContext::EditorContext(QObject* parent) : QObject(parent), scene_(proto::makeDefaultScene()) {
    // Undo, redo, and clicking a line in the Undo History panel all move the
    // stack's index; every one of those changes the scene.
    connect(&undoStack_, &QUndoStack::indexChanged, this, [this] {
        if (!pushing_) changed();
    });
}

void EditorContext::execute(std::unique_ptr<proto::Command> command) {
    // Whether or not Qt reports a change for a merged command, send exactly one
    // sceneChanged() per edit.
    pushing_ = true;
    undoStack_.push(new CoreUndoCommand(this, std::move(command)));
    pushing_ = false;
    changed();
}

void EditorContext::changed() {
    // If the selected object was deleted (or its creation undone), drop the selection.
    if (selected_ != 0 && !scene_.findObject(selected_)) {
        selected_ = 0;
        emit selectionChanged();
    }
    emit sceneChanged();
}

void EditorContext::replaceScene(proto::Scene scene) {
    pushing_ = true;  // clear() below reports an index change; one notification is enough
    scene_ = std::move(scene);
    pixmaps_.clear();  // asset IDs may mean different images in the new scene
    undoStack_.clear();
    pushing_ = false;
    selected_ = 0;
    emit selectionChanged();
    emit sceneChanged();
}

void EditorContext::select(proto::ObjectId id) {
    if (id == selected_) return;
    selected_ = id;
    emit selectionChanged();
}

QPixmap EditorContext::pixmapFor(proto::AssetId id) {
    auto found = pixmaps_.find(id);
    if (found != pixmaps_.end()) return found->second;
    const proto::Asset* asset = scene_.findAsset(id);
    if (!asset || asset->gbaPreview.pixels.empty()) return {};
    const proto::ImageRgba& image = asset->gbaPreview;
    // QImage::Format_RGBA8888 = bytes in memory are R, G, B, A: the core's layout.
    // .copy() makes QImage own its pixels instead of pointing into the asset.
    QImage qimage(image.pixels.data(), image.width, image.height, image.width * 4, QImage::Format_RGBA8888);
    QPixmap pixmap = QPixmap::fromImage(qimage.copy());
    pixmaps_[id] = pixmap;
    return pixmap;
}

void EditorContext::log(const QString& message) {
    qInfo("%s", qPrintable(message));
    emit logMessage(message);
}
