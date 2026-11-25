#pragma once
// =============================================================================
// proto_core/commands.hpp — every edit is a "command" that can be undone.
//
// THE IDEA: instead of changing the scene directly, the UI creates a small
// object that knows (1) how to make the change and (2) how to take it back.
// Keep a list of the commands you ran, and "undo" is just: take the last one
// off the list and call revert(). This is called the Command pattern.
//
// Analogy: a diary of everything you changed, written so precisely that you can
// read it backwards and put everything back the way it was.
//
// Both prototypes use these SAME command classes. The ImGui prototype stores them
// in proto::CommandHistory (below). The Qt prototype wraps each one in a Qt
// QUndoCommand so it can use Qt's QUndoStack and its free "history" panel.
// Either way, the actual editing logic lives here, outside any UI framework.
// =============================================================================

#include "proto_core/model.hpp"

#include <memory>
#include <string>
#include <vector>

namespace proto {

class Command {
public:
    virtual ~Command() = default;

    virtual void apply(Scene& scene) = 0;   // do (or redo) the change
    virtual void revert(Scene& scene) = 0;  // undo the change
    virtual std::string description() const = 0;  // e.g. "Move Player"

    // Merging: dragging an object produces dozens of tiny moves per second. We
    // want ONE undo step for the whole drag, not fifty. So a command may absorb
    // the next command if they belong together (same object, same drag).
    // mergeId() < 0 means "never merges". Two commands can only merge if they
    // report the same mergeId (Qt's QUndoStack uses exactly this rule too).
    virtual int mergeId() const { return -1; }
    virtual bool mergeWith(const Command& /*newer*/) { return false; }
};

// ----------------------------------------------------------------------------
// The undo/redo list used by the ImGui prototype (and by the tests).
// ----------------------------------------------------------------------------
class CommandHistory {
public:
    // Runs the command and records it. Recording a new command clears the redo
    // list, because "redo" only makes sense right after an undo.
    void execute(std::unique_ptr<Command> command, Scene& scene);

    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }
    void undo(Scene& scene);
    void redo(Scene& scene);
    void clear();

    std::string undoText() const;  // description of what Undo would undo
    std::string redoText() const;
    int undoCount() const { return static_cast<int>(undoStack_.size()); }

    // Increases every time the scene changes through this history. UIs can compare
    // it with the value they saw last time to know whether to refresh something
    // expensive (like regenerating the source-code preview).
    int revision() const { return revision_; }

private:
    std::vector<std::unique_ptr<Command>> undoStack_;
    std::vector<std::unique_ptr<Command>> redoStack_;
    int revision_ = 0;
};

// ----------------------------------------------------------------------------
// The concrete edits. Each one stores whatever it needs to put things back.
// ----------------------------------------------------------------------------

// Creates a GameObject. If `spriteAsset` is not 0, it also gets a SpriteRenderer
// showing that asset (used when an asset is dropped onto empty viewport space).
class CreateObjectCommand : public Command {
public:
    CreateObjectCommand(std::string name, int x, int y, AssetId spriteAsset = 0);
    void apply(Scene& scene) override;
    void revert(Scene& scene) override;
    std::string description() const override { return "Create " + name_; }
    ObjectId createdId() const { return id_; }

private:
    std::string name_;
    int x_, y_;
    AssetId spriteAsset_;
    ObjectId id_ = 0;  // chosen on the first apply, reused on redo
};

class DeleteObjectCommand : public Command {
public:
    explicit DeleteObjectCommand(ObjectId id) : id_(id) {}
    void apply(Scene& scene) override;
    void revert(Scene& scene) override;
    std::string description() const override { return "Delete " + saved_.name; }

private:
    ObjectId id_;
    GameObject saved_;  // full copy, so undo can bring it back exactly
    int savedIndex_ = -1;
};

class RenameObjectCommand : public Command {
public:
    RenameObjectCommand(ObjectId id, std::string newName)
        : id_(id), newName_(std::move(newName)) {}
    void apply(Scene& scene) override;
    void revert(Scene& scene) override;
    std::string description() const override { return "Rename to " + newName_; }

private:
    ObjectId id_;
    std::string newName_;
    std::string oldName_;
};

// Sets x, y and z. `dragSession` groups commands for merging: the UI picks a new
// session number each time the user starts a new drag (or starts editing a field).
class SetPositionCommand : public Command {
public:
    SetPositionCommand(ObjectId id, int x, int y, int z, int dragSession);
    void apply(Scene& scene) override;
    void revert(Scene& scene) override;
    std::string description() const override { return "Move " + objectName_; }
    int mergeId() const override { return 1; }
    bool mergeWith(const Command& newer) override;

private:
    ObjectId id_;
    int newX_, newY_, newZ_;
    int oldX_ = 0, oldY_ = 0, oldZ_ = 0;
    int session_;
    std::string objectName_;
};

// Changes one property value of one component (found by component type).
class SetPropertyCommand : public Command {
public:
    SetPropertyCommand(ObjectId id, std::string componentType, int propertyIndex,
                       std::int64_t newValue, int editSession);
    void apply(Scene& scene) override;
    void revert(Scene& scene) override;
    std::string description() const override;
    int mergeId() const override { return 2; }
    bool mergeWith(const Command& newer) override;

private:
    ObjectId id_;
    std::string componentType_;
    int propertyIndex_;
    std::int64_t newValue_;
    std::int64_t oldValue_ = 0;
    int session_;
};

class AddComponentCommand : public Command {
public:
    AddComponentCommand(ObjectId id, std::string componentType)
        : id_(id), type_(std::move(componentType)) {}
    void apply(Scene& scene) override;
    void revert(Scene& scene) override;
    std::string description() const override { return "Add " + type_; }

private:
    ObjectId id_;
    std::string type_;
};

class RemoveComponentCommand : public Command {
public:
    RemoveComponentCommand(ObjectId id, std::string componentType)
        : id_(id), type_(std::move(componentType)) {}
    void apply(Scene& scene) override;
    void revert(Scene& scene) override;
    std::string description() const override { return "Remove " + type_; }

private:
    ObjectId id_;
    std::string type_;
    Component saved_;
    int savedIndex_ = -1;
};

// Drag-and-drop of an asset onto an object: sets its SpriteRenderer's sprite,
// adding a SpriteRenderer first if the object doesn't have one.
class AssignSpriteCommand : public Command {
public:
    AssignSpriteCommand(ObjectId id, AssetId asset) : id_(id), asset_(asset) {}
    void apply(Scene& scene) override;
    void revert(Scene& scene) override;
    std::string description() const override { return "Assign sprite"; }

private:
    ObjectId id_;
    AssetId asset_;
    bool addedComponent_ = false;
    std::int64_t oldAsset_ = 0;
};

// Raises or lowers one isometric tile.
class SetIsoHeightCommand : public Command {
public:
    SetIsoHeightCommand(int tileX, int tileY, int newHeight)
        : tileX_(tileX), tileY_(tileY), newHeight_(newHeight) {}
    void apply(Scene& scene) override;
    void revert(Scene& scene) override;
    std::string description() const override { return "Change tile height"; }

private:
    int tileX_, tileY_, newHeight_;
    int oldHeight_ = 0;
};

// Builds a component with every property set to its descriptor's default value.
// Returns a component with an empty `type` if the type is unknown.
Component makeDefaultComponent(const std::string& type);

}  // namespace proto
