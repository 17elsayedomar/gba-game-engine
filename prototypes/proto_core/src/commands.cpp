#include "proto_core/commands.hpp"

#include "proto_core/components.hpp"

#include <algorithm>

namespace proto {

// ============================================================================
// CommandHistory
// ============================================================================

void CommandHistory::execute(std::unique_ptr<Command> command, Scene& scene) {
    command->apply(scene);
    ++revision_;
    redoStack_.clear();

    // Try to fold this command into the previous one (e.g. one long drag).
    if (!undoStack_.empty()) {
        Command& previous = *undoStack_.back();
        if (previous.mergeId() >= 0 && previous.mergeId() == command->mergeId() &&
            previous.mergeWith(*command)) {
            return;  // merged: the newer command is no longer needed
        }
    }
    undoStack_.push_back(std::move(command));
}

void CommandHistory::undo(Scene& scene) {
    if (undoStack_.empty()) return;
    std::unique_ptr<Command> command = std::move(undoStack_.back());
    undoStack_.pop_back();
    command->revert(scene);
    redoStack_.push_back(std::move(command));
    ++revision_;
}

void CommandHistory::redo(Scene& scene) {
    if (redoStack_.empty()) return;
    std::unique_ptr<Command> command = std::move(redoStack_.back());
    redoStack_.pop_back();
    command->apply(scene);
    undoStack_.push_back(std::move(command));
    ++revision_;
}

void CommandHistory::clear() {
    undoStack_.clear();
    redoStack_.clear();
    ++revision_;
}

std::string CommandHistory::undoText() const {
    return undoStack_.empty() ? std::string() : undoStack_.back()->description();
}

std::string CommandHistory::redoText() const {
    return redoStack_.empty() ? std::string() : redoStack_.back()->description();
}

// ============================================================================
// Helpers
// ============================================================================

Component makeDefaultComponent(const std::string& type) {
    Component component;
    const ComponentDescriptor* descriptor = findComponentDescriptor(type);
    if (!descriptor) return component;  // unknown type: empty component
    component.type = type;
    for (const PropertyDescriptor& property : descriptor->properties) {
        component.values.push_back(property.defaultValue);
    }
    return component;
}

// Every command below follows the same safety rule: if the object it refers to
// no longer exists, it quietly does nothing instead of crashing.
//
// Another shared pattern: apply() always records the "old" value right before
// changing it. Undo/redo always runs commands in reverse/forward order, so the
// scene is always in exactly the state the command expects.

// ============================================================================
// CreateObjectCommand
// ============================================================================

CreateObjectCommand::CreateObjectCommand(std::string name, int x, int y, AssetId spriteAsset)
    : name_(std::move(name)), x_(x), y_(y), spriteAsset_(spriteAsset) {}

void CreateObjectCommand::apply(Scene& scene) {
    if (id_ == 0) id_ = scene.allocateId();  // first time only; redo keeps the same ID

    GameObject object;
    object.id = id_;
    object.name = name_;
    object.x = x_;
    object.y = y_;
    if (spriteAsset_ != 0) {
        Component sprite = makeDefaultComponent(sprite_renderer::kType);
        sprite.values[sprite_renderer::kSprite] = spriteAsset_;
        object.components.push_back(sprite);
    }
    scene.objects.push_back(std::move(object));
}

void CreateObjectCommand::revert(Scene& scene) {
    int index = scene.indexOfObject(id_);
    if (index >= 0) scene.objects.erase(scene.objects.begin() + index);
}

// ============================================================================
// DeleteObjectCommand
// ============================================================================

void DeleteObjectCommand::apply(Scene& scene) {
    savedIndex_ = scene.indexOfObject(id_);
    if (savedIndex_ < 0) return;
    saved_ = scene.objects[savedIndex_];
    scene.objects.erase(scene.objects.begin() + savedIndex_);
}

void DeleteObjectCommand::revert(Scene& scene) {
    if (savedIndex_ < 0) return;
    // Put it back in the same place in the list, so the Hierarchy order is restored.
    int index = std::min(savedIndex_, static_cast<int>(scene.objects.size()));
    scene.objects.insert(scene.objects.begin() + index, saved_);
}

// ============================================================================
// RenameObjectCommand
// ============================================================================

void RenameObjectCommand::apply(Scene& scene) {
    if (GameObject* object = scene.findObject(id_)) {
        oldName_ = object->name;
        object->name = newName_;
    }
}

void RenameObjectCommand::revert(Scene& scene) {
    if (GameObject* object = scene.findObject(id_)) object->name = oldName_;
}

// ============================================================================
// SetPositionCommand
// ============================================================================

SetPositionCommand::SetPositionCommand(ObjectId id, int x, int y, int z, int dragSession)
    : id_(id), newX_(x), newY_(y), newZ_(z), session_(dragSession) {}

void SetPositionCommand::apply(Scene& scene) {
    if (GameObject* object = scene.findObject(id_)) {
        oldX_ = object->x;
        oldY_ = object->y;
        oldZ_ = object->z;
        objectName_ = object->name;
        object->x = newX_;
        object->y = newY_;
        object->z = newZ_;
    }
}

void SetPositionCommand::revert(Scene& scene) {
    if (GameObject* object = scene.findObject(id_)) {
        object->x = oldX_;
        object->y = oldY_;
        object->z = oldZ_;
    }
}

bool SetPositionCommand::mergeWith(const Command& newer) {
    // mergeId() matched, so `newer` is also a SetPositionCommand.
    const auto& other = static_cast<const SetPositionCommand&>(newer);
    if (other.id_ != id_ || other.session_ != session_) return false;
    // Keep OUR old position (where the drag started) and take THEIR new one.
    newX_ = other.newX_;
    newY_ = other.newY_;
    newZ_ = other.newZ_;
    return true;
}

// ============================================================================
// SetPropertyCommand
// ============================================================================

SetPropertyCommand::SetPropertyCommand(ObjectId id, std::string componentType,
                                       int propertyIndex, std::int64_t newValue,
                                       int editSession)
    : id_(id),
      componentType_(std::move(componentType)),
      propertyIndex_(propertyIndex),
      newValue_(newValue),
      session_(editSession) {}

void SetPropertyCommand::apply(Scene& scene) {
    GameObject* object = scene.findObject(id_);
    Component* component = object ? object->findComponent(componentType_) : nullptr;
    if (!component || propertyIndex_ >= static_cast<int>(component->values.size())) return;
    oldValue_ = component->values[propertyIndex_];
    component->values[propertyIndex_] = newValue_;
}

void SetPropertyCommand::revert(Scene& scene) {
    GameObject* object = scene.findObject(id_);
    Component* component = object ? object->findComponent(componentType_) : nullptr;
    if (!component || propertyIndex_ >= static_cast<int>(component->values.size())) return;
    component->values[propertyIndex_] = oldValue_;
}

std::string SetPropertyCommand::description() const {
    const ComponentDescriptor* descriptor = findComponentDescriptor(componentType_);
    if (descriptor && propertyIndex_ < static_cast<int>(descriptor->properties.size())) {
        return "Change " + descriptor->properties[propertyIndex_].name;
    }
    return "Change property";
}

bool SetPropertyCommand::mergeWith(const Command& newer) {
    const auto& other = static_cast<const SetPropertyCommand&>(newer);
    if (other.id_ != id_ || other.session_ != session_ ||
        other.componentType_ != componentType_ || other.propertyIndex_ != propertyIndex_) {
        return false;
    }
    newValue_ = other.newValue_;
    return true;
}

// ============================================================================
// Add / remove components
// ============================================================================

void AddComponentCommand::apply(Scene& scene) {
    GameObject* object = scene.findObject(id_);
    if (!object || object->findComponent(type_)) return;  // one of each type at most
    Component component = makeDefaultComponent(type_);
    if (!component.type.empty()) object->components.push_back(component);
}

void AddComponentCommand::revert(Scene& scene) {
    GameObject* object = scene.findObject(id_);
    if (!object) return;
    auto& list = object->components;
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const Component& c) { return c.type == type_; }),
               list.end());
}

void RemoveComponentCommand::apply(Scene& scene) {
    savedIndex_ = -1;
    GameObject* object = scene.findObject(id_);
    if (!object) return;
    for (int i = 0; i < static_cast<int>(object->components.size()); ++i) {
        if (object->components[i].type == type_) {
            saved_ = object->components[i];
            savedIndex_ = i;
            object->components.erase(object->components.begin() + i);
            return;
        }
    }
}

void RemoveComponentCommand::revert(Scene& scene) {
    GameObject* object = scene.findObject(id_);
    if (!object || savedIndex_ < 0) return;
    int index = std::min(savedIndex_, static_cast<int>(object->components.size()));
    object->components.insert(object->components.begin() + index, saved_);
}

// ============================================================================
// AssignSpriteCommand
// ============================================================================

void AssignSpriteCommand::apply(Scene& scene) {
    addedComponent_ = false;
    GameObject* object = scene.findObject(id_);
    if (!object) return;
    Component* sprite = object->findComponent(sprite_renderer::kType);
    if (!sprite) {
        object->components.push_back(makeDefaultComponent(sprite_renderer::kType));
        sprite = &object->components.back();
        addedComponent_ = true;
    }
    oldAsset_ = sprite->values[sprite_renderer::kSprite];
    sprite->values[sprite_renderer::kSprite] = asset_;
}

void AssignSpriteCommand::revert(Scene& scene) {
    GameObject* object = scene.findObject(id_);
    if (!object) return;
    if (addedComponent_) {
        RemoveComponentCommand(id_, sprite_renderer::kType).apply(scene);
    } else if (Component* sprite = object->findComponent(sprite_renderer::kType)) {
        sprite->values[sprite_renderer::kSprite] = oldAsset_;
    }
}

// ============================================================================
// SetIsoHeightCommand
// ============================================================================

void SetIsoHeightCommand::apply(Scene& scene) {
    IsoMap& map = scene.iso;
    if (tileX_ < 0 || tileY_ < 0 || tileX_ >= map.width || tileY_ >= map.depth) return;
    oldHeight_ = map.heightAt(tileX_, tileY_);
    map.setHeight(tileX_, tileY_, std::clamp(newHeight_, 0, IsoMap::kMaxHeight));
}

void SetIsoHeightCommand::revert(Scene& scene) {
    IsoMap& map = scene.iso;
    if (tileX_ < 0 || tileY_ < 0 || tileX_ >= map.width || tileY_ >= map.depth) return;
    map.setHeight(tileX_, tileY_, oldHeight_);
}

}  // namespace proto
