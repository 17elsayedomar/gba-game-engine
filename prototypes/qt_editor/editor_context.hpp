#pragma once
// =============================================================================
// EditorContext — the one place in the Qt prototype that owns the scene.
//
// HOW RETAINED-MODE UI WORKS (the key idea of this prototype):
// Qt widgets are long-lived objects that each keep their own copy of what they
// show: a QSpinBox remembers its number, a QTreeView asks its model for rows and
// caches them. So when the scene changes (an edit, an undo, loading a file),
// every panel must be TOLD to refresh. That is what the signals below are for:
//
//     some panel  --execute(command)-->  EditorContext  --sceneChanged()-->  every panel refreshes
//
// Compare with the ImGui prototype, where the whole UI is simply redrawn from
// the scene every frame and no refresh signals are needed.
//
// The scene, the commands and everything they do come from proto_core. This
// class only connects them to Qt.
// =============================================================================

#include "proto_core/commands.hpp"
#include "proto_core/model.hpp"

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QUndoCommand>
#include <QUndoStack>

#include <map>
#include <memory>

class EditorContext : public QObject {
    Q_OBJECT
public:
    explicit EditorContext(QObject* parent = nullptr);

    proto::Scene& scene() { return scene_; }
    const proto::Scene& scene() const { return scene_; }
    QUndoStack* undoStack() { return &undoStack_; }

    // Runs a proto_core command through Qt's undo stack.
    void execute(std::unique_ptr<proto::Command> command);

    // For changes that are not undoable (importing an asset, editing the node graph).
    void notifyChanged() { changed(); }

    // Replaces the whole scene (New / Open). Clears undo history and selection.
    void replaceScene(proto::Scene scene);

    proto::ObjectId selected() const { return selected_; }
    void select(proto::ObjectId id);

    // A new number for grouping edits into one undo step (see commands.hpp).
    int newSession() { return nextSession_++; }

    // The GBA-color preview of an asset as a QPixmap, created once and cached.
    QPixmap pixmapFor(proto::AssetId id);

    void log(const QString& message);

signals:
    void sceneChanged();      // anything in the scene may have changed
    void selectionChanged();  // a different object was selected
    void logMessage(const QString& message);

private:
    void changed();

    proto::Scene scene_;
    QUndoStack undoStack_;
    proto::ObjectId selected_ = 0;
    int nextSession_ = 1;
    bool pushing_ = false;
    std::map<proto::AssetId, QPixmap> pixmaps_;
};

// -----------------------------------------------------------------------------
// The adapter between proto_core and Qt's undo system.
//
// QUndoStack (Qt's undo/redo list) needs QUndoCommand objects. Instead of writing
// every edit twice, this one small class wraps ANY proto_core command. Qt's
// merging rule (same id() -> call mergeWith) is the same rule proto_core uses,
// so a drag still becomes one undo step.
// -----------------------------------------------------------------------------
class CoreUndoCommand : public QUndoCommand {
public:
    CoreUndoCommand(EditorContext* context, std::unique_ptr<proto::Command> command)
        : context_(context), command_(std::move(command)) {
        setText(QString::fromStdString(command_->description()));
    }
    void redo() override { command_->apply(context_->scene()); }  // Qt calls redo() on push, too
    void undo() override { command_->revert(context_->scene()); }
    int id() const override { return command_->mergeId(); }
    bool mergeWith(const QUndoCommand* other) override {
        // Qt only calls this when id() matched, so `other` is also a CoreUndoCommand.
        return command_->mergeWith(*static_cast<const CoreUndoCommand*>(other)->command_);
    }

private:
    EditorContext* context_;
    std::unique_ptr<proto::Command> command_;
};
