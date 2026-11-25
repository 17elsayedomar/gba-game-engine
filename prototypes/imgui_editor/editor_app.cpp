#include "editor_app.hpp"

#include "proto_core/assets.hpp"
#include "proto_core/codegen.hpp"
#include "proto_core/components.hpp"
#include "proto_core/geometry.hpp"
#include "proto_core/serialization.hpp"

#include <imgui_internal.h>     // DockBuilder API (building the default panel layout)
#include <misc/imgui_stdlib.h>  // InputText overload that edits a std::string

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <optional>

using namespace proto;

// The payload type name used for dragging assets. Any drop target that accepts
// this name knows the payload's bytes are one AssetId.
static constexpr const char* kAssetPayload = "GBE_ASSET";

// GBA screen size in pixels.
static constexpr int kGbaWidth = 240;
static constexpr int kGbaHeight = 160;

static ImVec2 operator+(ImVec2 a, ImVec2 b) { return {a.x + b.x, a.y + b.y}; }
static ImVec2 operator-(ImVec2 a, ImVec2 b) { return {a.x - b.x, a.y - b.y}; }

EditorApp::EditorApp(SDL_Window* window, SDL_Renderer* renderer)
    : window_(window), renderer_(renderer), scene_(makeDefaultScene()) {
    log("ImGui prototype started. Import a PNG from the Assets panel to begin.");
}

EditorApp::~EditorApp() { clearTextures(); }

// =============================================================================
// Frame
// =============================================================================

void EditorApp::drawFrame() {
    processDialogResults();

    // If the selected object was deleted (or un-created by undo), drop the selection.
    if (selected_ != 0 && !scene_.findObject(selected_)) selected_ = 0;

    // A dockspace covering the whole window: every panel below can be dragged
    // around and docked into it. ImGui remembers the layout in imgui.ini.
    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    if (resetLayout_ || ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
        buildDefaultLayout(dockspaceId);
        resetLayout_ = false;
    }
    drawMenuBar();
    ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport());

    drawHierarchy();
    drawAssets();
    drawInspector();
    drawViewport();
    drawIsoGrid();
    drawNodeGraph();
    drawSourcePanel();
    drawConsole();
    handleShortcuts();
}

void EditorApp::buildDefaultLayout(ImGuiID id) {
    // Split the window into regions, like cutting a sheet of paper:
    //   [Hierarchy/Assets] [ Scene | Isometric | Node Graph ] [Inspector]
    //                      [ Generated Source | Console      ]
    ImGui::DockBuilderRemoveNode(id);
    ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id, ImGui::GetMainViewport()->Size);
    ImGuiID center = id;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.32f, nullptr, &center);
    ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.45f, nullptr, &left);
    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Assets", leftBottom);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Scene", center);
    ImGui::DockBuilderDockWindow("Isometric Grid", center);
    ImGui::DockBuilderDockWindow("Node Graph", center);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Generated Source", bottom);  // docked last = the visible tab
    ImGui::DockBuilderFinish(id);
}

// =============================================================================
// Actions
// =============================================================================

void EditorApp::execute(std::unique_ptr<Command> command) {
    history_.execute(std::move(command), scene_);
    markChanged();
}

void EditorApp::undo() {
    if (!history_.canUndo()) return;
    log("Undo: " + history_.undoText());
    history_.undo(scene_);
    markChanged();
}

void EditorApp::redo() {
    if (!history_.canRedo()) return;
    log("Redo: " + history_.redoText());
    history_.redo(scene_);
    markChanged();
}

void EditorApp::newScene() {
    clearTextures();
    scene_ = makeDefaultScene();
    history_.clear();
    scenePath_.clear();
    selected_ = 0;
    markChanged();
    log("New scene.");
}

bool EditorApp::openScene(const std::string& path) {
    std::string error;
    auto loaded = loadSceneFile(path, error);
    if (!loaded) {
        log("Open failed: " + error);
        return false;
    }
    clearTextures();  // asset IDs may mean different images in the new scene
    scene_ = std::move(*loaded);
    history_.clear();
    scenePath_ = path;
    selected_ = 0;
    markChanged();
    log("Opened " + path);
    return true;
}

void EditorApp::saveScene(const std::string& path) {
    std::string error;
    if (saveSceneFile(scene_, path, error)) {
        scenePath_ = path;
        log("Saved " + path);
    } else {
        log("Save failed: " + error);
    }
}

void EditorApp::importAsset(const std::string& path) {
    std::string error;
    auto asset = importPng(path, error);
    if (!asset) {
        log("Import failed: " + error);
        return;
    }
    asset->id = scene_.allocateId();
    log("Imported " + asset->name + " (" + std::to_string(asset->gbaColorCount) + " GBA colors)");
    for (const std::string& warning : asset->warnings) log("  Warning: " + warning);
    scene_.assets.push_back(std::move(*asset));
    markChanged();
}

void EditorApp::createObject() {
    auto command = std::make_unique<CreateObjectCommand>(
        "GameObject " + std::to_string(scene_.objects.size() + 1), 112, 72);
    CreateObjectCommand* raw = command.get();
    execute(std::move(command));
    selected_ = raw->createdId();
}

void EditorApp::deleteSelected() {
    if (selected_ != 0) execute(std::make_unique<DeleteObjectCommand>(selected_));
}

void EditorApp::selectFirstObject() {
    if (!scene_.objects.empty()) selected_ = scene_.objects.front().id;
}

void EditorApp::benchmarkStep(int frameNumber) {
    if (selected_ == 0) return;
    if (benchmarkSession_ == 0) benchmarkSession_ = nextSession_++;
    const GameObject* object = scene_.findObject(selected_);
    double t = frameNumber * 0.05;
    int x = 112 + static_cast<int>(std::lround(80 * std::cos(t)));
    int y = 72 + static_cast<int>(std::lround(50 * std::sin(t)));
    execute(std::make_unique<SetPositionCommand>(selected_, x, y, object->z, benchmarkSession_));
}

void EditorApp::log(const std::string& message) {
    log_.push_back(message);
    SDL_Log("%s", message.c_str());
}

// =============================================================================
// Menus and keyboard shortcuts
// =============================================================================

void EditorApp::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene")) newScene();
        if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) showDialog(DialogKind::OpenScene);
        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
            if (scenePath_.empty()) showDialog(DialogKind::SaveScene);
            else saveScene(scenePath_);
        }
        if (ImGui::MenuItem("Save Scene As...")) showDialog(DialogKind::SaveScene);
        ImGui::Separator();
        if (ImGui::MenuItem("Import PNG...")) showDialog(DialogKind::ImportPng);
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) quitRequested_ = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        std::string undoLabel = "Undo " + history_.undoText();
        std::string redoLabel = "Redo " + history_.redoText();
        if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, history_.canUndo())) undo();
        if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, history_.canRedo())) redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Create GameObject", "Ctrl+N")) createObject();
        if (ImGui::MenuItem("Delete", "Del", false, selected_ != 0)) deleteSelected();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Reset Layout")) resetLayout_ = true;
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void EditorApp::handleShortcuts() {
    // While the user is typing in a text field, Ctrl+Z belongs to that field.
    if (ImGui::GetIO().WantTextInput) return;
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) undo();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) ||
        ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) redo();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) createObject();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O)) showDialog(DialogKind::OpenScene);
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
        if (scenePath_.empty()) showDialog(DialogKind::SaveScene);
        else saveScene(scenePath_);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deleteSelected();
}

// =============================================================================
// Hierarchy
// =============================================================================

void EditorApp::acceptAssetDropOnObject(ObjectId target) {
    // Call right after the item that should accept drops.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetPayload)) {
            AssetId asset = *static_cast<const AssetId*>(payload->Data);
            execute(std::make_unique<AssignSpriteCommand>(target, asset));
            selected_ = target;
        }
        ImGui::EndDragDropTarget();
    }
}

void EditorApp::drawHierarchy() {
    ImGui::Begin("Hierarchy");
    if (ImGui::Button("+ Create")) createObject();
    ImGui::SameLine();
    ImGui::BeginDisabled(selected_ == 0);
    if (ImGui::Button("Delete")) deleteSelected();
    ImGui::EndDisabled();
    ImGui::Separator();

    ObjectId deleteRequest = 0;
    for (const GameObject& object : scene_.objects) {
        // PushID makes every row's widgets unique even if two objects share a name.
        ImGui::PushID(static_cast<int>(object.id));
        const char* label = object.name.empty() ? "(unnamed)" : object.name.c_str();
        if (ImGui::Selectable(label, object.id == selected_)) selected_ = object.id;
        acceptAssetDropOnObject(object.id);  // drop a sprite onto a row to assign it
        if (ImGui::BeginPopupContextItem()) {  // right-click menu
            if (ImGui::MenuItem("Delete")) deleteRequest = object.id;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    // Delete after the loop: changing scene_.objects while looping over it would
    // invalidate the loop.
    if (deleteRequest != 0) execute(std::make_unique<DeleteObjectCommand>(deleteRequest));

    if (scene_.objects.empty()) ImGui::TextDisabled("No objects. Click + Create.");
    ImGui::End();
}

// =============================================================================
// Inspector — built entirely from the component descriptors
// =============================================================================

void EditorApp::drawInspector() {
    ImGui::Begin("Inspector");
    GameObject* object = scene_.findObject(selected_);
    if (!object) {
        ImGui::TextDisabled("Select an object in the Hierarchy or the Scene.");
        ImGui::End();
        return;
    }

    // --- Name: committed as one Rename command when the user leaves the field.
    if (renameTarget_ != object->id || !renameActive_) renameBuffer_ = object->name;
    renameTarget_ = object->id;
    ImGui::InputText("Name", &renameBuffer_);
    renameActive_ = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit() && renameBuffer_ != object->name) {
        execute(std::make_unique<RenameObjectCommand>(object->id, renameBuffer_));
    }

    // --- Transform: X, Y, Z. Each drag of a field becomes one undo step.
    ImGui::SeparatorText("Transform");
    int values[3] = {object->x, object->y, object->z};
    const char* labels[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i) {
        bool changed = ImGui::DragInt(labels[i], &values[i], 0.25f, -512, 512);
        if (ImGui::IsItemActivated()) positionSession_ = nextSession_++;
        if (changed) {
            execute(std::make_unique<SetPositionCommand>(object->id, values[0], values[1], values[2],
                                                         positionSession_));
            object = scene_.findObject(selected_);  // re-fetch: execute() may move memory
        }
    }

    // --- Components: one collapsible section per component, fields from the descriptor.
    std::unique_ptr<Command> pending;  // run after the loop, for the same reason as above
    for (const Component& component : object->components) {
        const ComponentDescriptor* descriptor = findComponentDescriptor(component.type);
        if (!descriptor) continue;
        ImGui::PushID(component.type.c_str());
        bool keep = true;  // the header's [x] button sets this to false
        if (ImGui::CollapsingHeader(descriptor->displayName.c_str(), &keep,
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            for (int i = 0; i < static_cast<int>(descriptor->properties.size()); ++i) {
                const PropertyDescriptor& property = descriptor->properties[i];
                ImGui::PushID(i);
                if (property.kind == PropertyKind::Int) {
                    int value = static_cast<int>(component.values[i]);
                    bool changed = ImGui::DragInt(property.name.c_str(), &value, 0.1f,
                                                  property.minValue, property.maxValue);
                    if (ImGui::IsItemActivated()) propertySession_ = nextSession_++;
                    if (changed)
                        pending = std::make_unique<SetPropertyCommand>(object->id, component.type, i,
                                                                       value, propertySession_);
                } else {  // AssetRef: a drop-down list, which also accepts dragged assets
                    const Asset* current = scene_.findAsset(component.values[i]);
                    if (ImGui::BeginCombo(property.name.c_str(), current ? current->name.c_str() : "(none)")) {
                        if (ImGui::Selectable("(none)", current == nullptr))
                            pending = std::make_unique<SetPropertyCommand>(object->id, component.type, i, 0,
                                                                           nextSession_++);
                        for (const Asset& asset : scene_.assets) {
                            ImGui::PushID(static_cast<int>(asset.id));
                            if (ImGui::Selectable(asset.name.c_str(), current == &asset))
                                pending = std::make_unique<SetPropertyCommand>(object->id, component.type, i,
                                                                               asset.id, nextSession_++);
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kAssetPayload))
                            pending = std::make_unique<SetPropertyCommand>(
                                object->id, component.type, i, *static_cast<const AssetId*>(p->Data),
                                nextSession_++);
                        ImGui::EndDragDropTarget();
                    }
                }
                if (!property.help.empty() && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", property.help.c_str());
                ImGui::PopID();
            }
        }
        if (!keep) pending = std::make_unique<RemoveComponentCommand>(object->id, component.type);
        ImGui::PopID();
    }
    if (pending) execute(std::move(pending));

    // --- Add Component: lists only the types this object doesn't have yet.
    ImGui::Spacing();
    if (ImGui::Button("Add Component")) ImGui::OpenPopup("AddComponentPopup");
    if (ImGui::BeginPopup("AddComponentPopup")) {
        GameObject* current = scene_.findObject(selected_);
        for (const ComponentDescriptor& descriptor : componentRegistry()) {
            if (current && current->findComponent(descriptor.type)) continue;
            if (ImGui::MenuItem(descriptor.displayName.c_str()))
                execute(std::make_unique<AddComponentCommand>(selected_, descriptor.type));
        }
        ImGui::EndPopup();
    }
    ImGui::End();
}

// =============================================================================
// Assets
// =============================================================================

SDL_Texture* EditorApp::textureFor(const Asset& asset) {
    if (asset.gbaPreview.pixels.empty()) return nullptr;
    auto found = textures_.find(asset.id);
    if (found != textures_.end()) return found->second;

    // Wrap the shared core's RGBA pixels in an SDL surface, then upload to the GPU.
    // SDL_PIXELFORMAT_RGBA32 means "bytes in memory are R, G, B, A" — exactly the
    // core's layout.
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        asset.gbaPreview.width, asset.gbaPreview.height, SDL_PIXELFORMAT_RGBA32,
        const_cast<std::uint8_t*>(asset.gbaPreview.pixels.data()), asset.gbaPreview.width * 4);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);
    // Nearest-neighbor scaling keeps pixel art crisp instead of blurry when zoomed.
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    textures_[asset.id] = texture;
    return texture;
}

void EditorApp::clearTextures() {
    for (auto& [id, texture] : textures_) SDL_DestroyTexture(texture);
    textures_.clear();
}

void EditorApp::drawAssets() {
    ImGui::Begin("Assets");
    if (ImGui::Button("Import PNG...")) showDialog(DialogKind::ImportPng);
    ImGui::TextDisabled("Drag an asset onto an object or into the Scene.");
    ImGui::Separator();
    const float thumb = 48.0f * ImGui::GetStyle().FontScaleDpi;
    for (const Asset& asset : scene_.assets) {
        ImGui::PushID(static_cast<int>(asset.id));
        SDL_Texture* texture = textureFor(asset);
        if (texture) {
            ImGui::ImageButton("thumb", (ImTextureID)(intptr_t)texture, ImVec2(thumb, thumb));
        } else {
            ImGui::Button("?", ImVec2(thumb, thumb));
        }
        // Make the thumbnail a drag source carrying the asset's ID.
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(kAssetPayload, &asset.id, sizeof(asset.id));
            if (texture) ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2(thumb, thumb));
            ImGui::Text("%s", asset.name.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text("%s", asset.name.c_str());
        ImGui::TextDisabled("%dx%d, %d GBA colors", asset.original.width, asset.original.height,
                            asset.gbaColorCount);
        for (const std::string& warning : asset.warnings)
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "%s", warning.c_str());
        ImGui::EndGroup();
        ImGui::PopID();
    }
    ImGui::End();
}

// =============================================================================
// Scene viewport
// =============================================================================

void EditorApp::drawViewport() {
    ImGui::Begin("Scene");
    ImGui::Checkbox("Snap to 8 px grid", &snapToGrid_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120 * ImGui::GetStyle().FontScaleDpi);
    ImGui::SliderFloat("Zoom", &zoom_, 1.0f, 6.0f, "%.0fx");
    zoom_ = std::round(zoom_);

    // One big invisible button is the canvas: it captures mouse clicks and drags.
    // We then draw everything ourselves with the window's draw list.
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    size.x = std::max(size.x, 50.0f);
    size.y = std::max(size.y, 50.0f);
    ImGui::InvisibleButton("canvas", size);
    const bool hovered = ImGui::IsItemHovered();

    ImVec2 screen0 = origin + ImVec2(16, 16);  // top-left of the GBA screen on the canvas
    // Zoom is in logical pixels (like Qt), so 3x on a 150% display would be 4.5
    // screen pixels per GBA pixel. Rounding to a whole number keeps every GBA
    // pixel exactly the same size, which matters for pixel art.
    const float zoom = std::max(1.0f, std::round(zoom_ * ImGui::GetStyle().FontScaleDpi));
    ImVec2 mouse = ImGui::GetIO().MousePos;
    int mouseGbaX = static_cast<int>(std::floor((mouse.x - screen0.x) / zoom));
    int mouseGbaY = static_cast<int>(std::floor((mouse.y - screen0.y) / zoom));

    // Dropping an asset: onto an object assigns it; onto empty space creates an object.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetPayload)) {
            AssetId assetId = *static_cast<const AssetId*>(payload->Data);
            ObjectId hit = pickObject(scene_, mouseGbaX, mouseGbaY);
            if (hit != 0) {
                execute(std::make_unique<AssignSpriteCommand>(hit, assetId));
                selected_ = hit;
            } else if (const Asset* asset = scene_.findAsset(assetId)) {
                std::string name = std::filesystem::path(asset->name).stem().string();
                auto command = std::make_unique<CreateObjectCommand>(
                    name, mouseGbaX - asset->original.width / 2, mouseGbaY - asset->original.height / 2,
                    assetId);
                CreateObjectCommand* raw = command.get();
                execute(std::move(command));
                selected_ = raw->createdId();
            }
        }
        ImGui::EndDragDropTarget();
    }

    // --- Mouse: click selects, drag moves (one undo step per drag).
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        selected_ = pickObject(scene_, mouseGbaX, mouseGbaY);
        if (const GameObject* object = scene_.findObject(selected_)) {
            draggingObject_ = true;
            positionSession_ = nextSession_++;
            dragOffsetX_ = mouseGbaX - object->x;
            dragOffsetY_ = mouseGbaY - object->y;
        }
    }
    if (draggingObject_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (const GameObject* object = scene_.findObject(selected_)) {
            int x = mouseGbaX - dragOffsetX_;
            int y = mouseGbaY - dragOffsetY_;
            if (snapToGrid_) {
                x = static_cast<int>(std::lround(x / 8.0)) * 8;
                y = static_cast<int>(std::lround(y / 8.0)) * 8;
            }
            if (x != object->x || y != object->y)
                execute(std::make_unique<SetPositionCommand>(selected_, x, y, object->z, positionSession_));
        }
    } else {
        draggingObject_ = false;
    }

    // --- Drawing.
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin, origin + size, true);
    draw->AddRectFilled(origin, origin + size, IM_COL32(45, 45, 52, 255));
    ImVec2 screen1 = screen0 + ImVec2(kGbaWidth * zoom, kGbaHeight * zoom);
    draw->AddRectFilled(screen0, screen1, IM_COL32(24, 28, 38, 255));
    if (snapToGrid_) {  // 8x8 grid = the GBA's tile size
        for (int gx = 8; gx < kGbaWidth; gx += 8)
            draw->AddLine(screen0 + ImVec2(gx * zoom, 0), ImVec2(screen0.x + gx * zoom, screen1.y),
                          IM_COL32(255, 255, 255, 18));
        for (int gy = 8; gy < kGbaHeight; gy += 8)
            draw->AddLine(screen0 + ImVec2(0, gy * zoom), ImVec2(screen1.x, screen0.y + gy * zoom),
                          IM_COL32(255, 255, 255, 18));
    }

    // Draw objects from lowest z to highest, so higher z appears on top.
    std::vector<const GameObject*> order;
    for (const GameObject& object : scene_.objects) order.push_back(&object);
    std::stable_sort(order.begin(), order.end(),
                     [](const GameObject* a, const GameObject* b) { return a->z < b->z; });
    for (const GameObject* object : order) {
        Point objectPixels = objectSize(scene_, *object);
        ImVec2 p0 = screen0 + ImVec2(object->x * zoom, object->y * zoom);
        ImVec2 p1 = p0 + ImVec2(objectPixels.x * zoom, objectPixels.y * zoom);
        const Component* sprite = object->findComponent(sprite_renderer::kType);
        const Asset* asset = sprite ? scene_.findAsset(sprite->values[sprite_renderer::kSprite]) : nullptr;
        SDL_Texture* texture = asset ? textureFor(*asset) : nullptr;
        if (texture) {
            draw->AddImage((ImTextureID)(intptr_t)texture, p0, p1);
        } else {
            draw->AddRectFilled(p0, p1, IM_COL32(90, 110, 160, 120));
            draw->AddRect(p0, p1, IM_COL32(150, 170, 220, 255));
        }
        if (object->id == selected_) {
            draw->AddRect(p0 - ImVec2(2, 2), p1 + ImVec2(2, 2), IM_COL32(255, 200, 0, 255), 0, 0, 2.0f);
            draw->AddText(p0 - ImVec2(0, ImGui::GetFontSize() + 4), IM_COL32(255, 200, 0, 255),
                          object->name.c_str());
        }
    }

    // The GBA screen border is drawn last so it is always visible.
    draw->AddRect(screen0 - ImVec2(1, 1), screen1 + ImVec2(1, 1), IM_COL32(255, 230, 80, 255), 0, 0, 2.0f);
    draw->AddText(ImVec2(screen0.x, screen1.y + 4), IM_COL32(255, 230, 80, 255),
                  "GBA screen 240x160 (pixels outside are off-screen)");
    if (hovered) {
        char text[64];
        SDL_snprintf(text, sizeof(text), "(%d, %d)", mouseGbaX, mouseGbaY);
        draw->AddText(ImVec2(screen1.x - 90, screen1.y + 4), IM_COL32(200, 200, 200, 255), text);
    }
    draw->PopClipRect();
    ImGui::End();
}

// =============================================================================
// Isometric grid (feature 13)
// =============================================================================

void EditorApp::drawIsoGrid() {
    ImGui::Begin("Isometric Grid");
    ImGui::TextWrapped("Left-click a tile to raise it, right-click to lower it (undoable). "
                       "Projection and picking come from proto_core/geometry.");
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    size.x = std::max(size.x, 50.0f);
    size.y = std::max(size.y, 50.0f);
    ImGui::InvisibleButton("iso", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    const IsoProjection projection;
    const float scale = 2.0f;  // draw each GBA pixel as 2x2 screen pixels
    ImVec2 anchor = origin + ImVec2(size.x * 0.5f, 80.0f);  // where tile (0,0) height 0 sits
    auto toScreen = [&](Point p) { return anchor + ImVec2(p.x * scale, p.y * scale); };

    ImVec2 mouse = ImGui::GetIO().MousePos;
    Point mouseIso{static_cast<int>(std::floor((mouse.x - anchor.x) / scale)),
                   static_cast<int>(std::floor((mouse.y - anchor.y) / scale))};
    std::optional<Point> hovered;
    if (ImGui::IsItemHovered()) hovered = isoPickTile(scene_.iso, mouseIso, projection);

    if (hovered) {
        int h = scene_.iso.heightAt(hovered->x, hovered->y);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            execute(std::make_unique<SetIsoHeightCommand>(hovered->x, hovered->y, h + 1));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            execute(std::make_unique<SetIsoHeightCommand>(hovered->x, hovered->y, h - 1));
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin, origin + size, true);
    for (Point tile : isoDrawOrder(scene_.iso)) {  // back to front
        IsoBlockFaces faces = isoBlockFaces(scene_.iso, tile.x, tile.y, projection);
        int h = scene_.iso.heightAt(tile.x, tile.y);
        ImVec2 top[4], left[4], right[4];
        for (int i = 0; i < 4; ++i) {
            top[i] = toScreen(faces.top[i]);
            left[i] = toScreen(faces.left[i]);
            right[i] = toScreen(faces.right[i]);
        }
        // Brighter tops for higher blocks; side walls darker to suggest light from above-left.
        int shade = 90 + h * 30;
        if (h > 0) {
            draw->AddConvexPolyFilled(left, 4, IM_COL32(shade * 6 / 10, shade * 5 / 10, 40, 255));
            draw->AddConvexPolyFilled(right, 4, IM_COL32(shade * 4 / 10, shade * 3 / 10, 30, 255));
        }
        draw->AddConvexPolyFilled(top, 4, IM_COL32(60, shade, 70, 255));
        bool isHovered = hovered && hovered->x == tile.x && hovered->y == tile.y;
        draw->AddPolyline(top, 4, isHovered ? IM_COL32(255, 230, 80, 255) : IM_COL32(0, 0, 0, 90),
                          isHovered ? 2.0f : 1.0f, ImDrawFlags_Closed);
    }
    if (hovered) {
        char text[64];
        SDL_snprintf(text, sizeof(text), "tile (%d, %d), height %d", hovered->x, hovered->y,
                     scene_.iso.heightAt(hovered->x, hovered->y));
        draw->AddText(origin + ImVec2(8, size.y - ImGui::GetFontSize() - 8), IM_COL32(230, 230, 230, 255), text);
    }
    draw->PopClipRect();
    ImGui::End();
}

// =============================================================================
// Node graph (feature 14)
// =============================================================================

void EditorApp::drawNodeGraph() {
    ImGui::Begin("Node Graph");
    ImGui::TextWrapped("Drag a node by its title. Drag from an output pin (right) to an input pin (left) "
                       "to connect. Drag a wire off an input to disconnect it.");
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    size.x = std::max(size.x, 50.0f);
    size.y = std::max(size.y, 50.0f);
    ImGui::InvisibleButton("graph", size);
    const float s = ImGui::GetStyle().FontScaleDpi;  // node sizes follow the UI scale
    auto toScreen = [&](Point p) { return origin + ImVec2(p.x * s, p.y * s); };
    ImVec2 mouse = ImGui::GetIO().MousePos;
    Point mouseGraph{static_cast<int>((mouse.x - origin.x) / s), static_cast<int>((mouse.y - origin.y) / s)};
    NodeGraph& graph = scene_.graph;

    // --- Mouse input.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        int nodeId = 0, pin = 0;
        bool isOutput = false;
        if (hitTestPin(graph, mouseGraph, nodeId, isOutput, pin)) {
            if (isOutput) {  // start a new wire
                linking_ = true;
                linkFromNode_ = nodeId;
                linkFromPin_ = pin;
            } else {  // pick up the wire already plugged into this input
                for (const GraphLink& link : graph.links) {
                    if (link.toNode == nodeId && link.toPin == pin) {
                        linking_ = true;
                        linkFromNode_ = link.fromNode;
                        linkFromPin_ = link.fromPin;
                    }
                }
                if (disconnectInput(graph, nodeId, pin)) markChanged();
            }
        } else {
            // Topmost node under the mouse = the last one in the list that contains it.
            for (const GraphNode& node : graph.nodes) {
                if (mouseGraph.x >= node.x && mouseGraph.x < node.x + NodeLayout::kWidth &&
                    mouseGraph.y >= node.y && mouseGraph.y < node.y + nodeHeight(node)) {
                    draggingNode_ = node.id;
                    nodeDragOffset_ = ImVec2(float(mouseGraph.x - node.x), float(mouseGraph.y - node.y));
                }
            }
        }
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (GraphNode* node = graph.findNode(draggingNode_)) {
            node->x = mouseGraph.x - static_cast<int>(nodeDragOffset_.x);
            node->y = mouseGraph.y - static_cast<int>(nodeDragOffset_.y);
        }
    } else {
        if (linking_) {  // mouse released: connect if it is over an input pin
            int nodeId = 0, pin = 0;
            bool isOutput = true;
            if (hitTestPin(graph, mouseGraph, nodeId, isOutput, pin) && !isOutput &&
                connectPins(graph, linkFromNode_, linkFromPin_, nodeId, pin)) {
                markChanged();
            }
        }
        linking_ = false;
        draggingNode_ = 0;
    }

    // --- Drawing: grid, wires, then nodes on top.
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin, origin + size, true);
    draw->AddRectFilled(origin, origin + size, IM_COL32(30, 30, 36, 255));
    for (float gx = 0; gx < size.x; gx += 32 * s)
        draw->AddLine(ImVec2(origin.x + gx, origin.y), ImVec2(origin.x + gx, origin.y + size.y), IM_COL32(255, 255, 255, 12));
    for (float gy = 0; gy < size.y; gy += 32 * s)
        draw->AddLine(ImVec2(origin.x, origin.y + gy), ImVec2(origin.x + size.x, origin.y + gy), IM_COL32(255, 255, 255, 12));

    auto drawWire = [&](Point from, Point to, ImU32 color) {
        auto c = linkCurve(from, to);
        draw->AddBezierCubic(toScreen(c[0]), toScreen(c[1]), toScreen(c[2]), toScreen(c[3]), color, 3.0f);
    };
    for (const GraphLink& link : graph.links) {
        const GraphNode* from = graph.findNode(link.fromNode);
        const GraphNode* to = graph.findNode(link.toNode);
        if (from && to)
            drawWire(pinPosition(*from, true, link.fromPin), pinPosition(*to, false, link.toPin),
                     IM_COL32(120, 200, 255, 255));
    }
    if (linking_) {
        if (const GraphNode* from = graph.findNode(linkFromNode_))
            drawWire(pinPosition(*from, true, linkFromPin_), mouseGraph, IM_COL32(255, 230, 80, 255));
    }

    for (const GraphNode& node : graph.nodes) {
        ImVec2 p0 = toScreen({node.x, node.y});
        ImVec2 p1 = toScreen({node.x + NodeLayout::kWidth, node.y + nodeHeight(node)});
        ImVec2 headerEnd = toScreen({node.x + NodeLayout::kWidth, node.y + NodeLayout::kHeaderHeight});
        draw->AddRectFilled(p0, p1, IM_COL32(55, 58, 70, 245), 6.0f);
        draw->AddRectFilled(p0, headerEnd, IM_COL32(80, 90, 140, 255), 6.0f, ImDrawFlags_RoundCornersTop);
        draw->AddRect(p0, p1, IM_COL32(0, 0, 0, 200), 6.0f);
        draw->AddText(p0 + ImVec2(8 * s, 5 * s), IM_COL32_WHITE, node.title.c_str());
        for (int pass = 0; pass < 2; ++pass) {
            const bool output = pass == 1;
            const auto& pins = output ? node.outputs : node.inputs;
            for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
                ImVec2 c = toScreen(pinPosition(node, output, i));
                draw->AddCircleFilled(c, NodeLayout::kPinRadius * s, IM_COL32(120, 200, 255, 255));
                ImVec2 textSize = ImGui::CalcTextSize(pins[i].c_str());
                ImVec2 textPos = output ? c - ImVec2(textSize.x + 12 * s, textSize.y / 2)
                                        : c + ImVec2(12 * s, -textSize.y / 2);
                draw->AddText(textPos, IM_COL32(220, 220, 220, 255), pins[i].c_str());
            }
        }
    }
    draw->PopClipRect();
    ImGui::End();
}

// =============================================================================
// Generated source (feature 12)
// =============================================================================

// ImGui has no syntax-highlighting text widget, so this tiny highlighter colors
// each line itself: comments, preprocessor lines, keywords, numbers, strings.
// (Qt has QSyntaxHighlighter for this; for ImGui a third-party widget such as
// ImGuiColorTextEdit would be the alternative.)
static void drawHighlightedLine(const std::string& line) {
    const ImU32 normal = IM_COL32(220, 220, 220, 255);
    const ImU32 comment = IM_COL32(110, 190, 110, 255);
    const ImU32 keyword = IM_COL32(110, 160, 255, 255);
    const ImU32 number = IM_COL32(230, 170, 90, 255);
    const ImU32 str = IM_COL32(220, 140, 120, 255);
    const ImU32 preprocessor = IM_COL32(200, 120, 220, 255);

    size_t first = line.find_first_not_of(' ');
    if (first != std::string::npos && line.compare(first, 2, "//") == 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, comment);
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
        return;
    }
    if (first != std::string::npos && line[first] == '#') {
        ImGui::PushStyleColor(ImGuiCol_Text, preprocessor);
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
        return;
    }
    static const char* kKeywords[] = {"static", "const", "extern", "nullptr", "int", "struct", "return"};
    size_t i = 0;
    bool firstPiece = true;
    auto piece = [&](size_t start, size_t end, ImU32 color) {
        if (!firstPiece) ImGui::SameLine(0.0f, 0.0f);
        firstPiece = false;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(line.c_str() + start, line.c_str() + end);
        ImGui::PopStyleColor();
    };
    while (i < line.size()) {
        size_t start = i;
        unsigned char ch = static_cast<unsigned char>(line[i]);
        if (std::isalpha(ch) || ch == '_') {
            while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) ++i;
            std::string word = line.substr(start, i - start);
            bool isKeyword = std::any_of(std::begin(kKeywords), std::end(kKeywords),
                                         [&](const char* k) { return word == k; });
            piece(start, i, isKeyword ? keyword : normal);
        } else if (std::isdigit(ch)) {
            while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
            piece(start, i, number);
        } else if (ch == '"') {
            ++i;
            while (i < line.size() && line[i] != '"') ++i;
            if (i < line.size()) ++i;
            piece(start, i, str);
        } else if (line.compare(i, 2, "//") == 0) {
            piece(i, line.size(), comment);  // comment at the end of a line
            i = line.size();
        } else {
            while (i < line.size() && !std::isalnum(static_cast<unsigned char>(line[i])) && line[i] != '_' &&
                   line[i] != '"' && line.compare(i, 2, "//") != 0)
                ++i;
            piece(start, i, normal);
        }
    }
    if (firstPiece) ImGui::TextUnformatted("");  // keep empty lines
}

void EditorApp::drawSourcePanel() {
    ImGui::Begin("Generated Source");
    if (sourceRevision_ != changeCounter_) {  // regenerate only after a change
        sourceText_ = generateSceneSource(scene_);
        sourceLines_.clear();
        size_t start = 0;
        while (start <= sourceText_.size()) {
            size_t end = sourceText_.find('\n', start);
            if (end == std::string::npos) end = sourceText_.size();
            sourceLines_.push_back(sourceText_.substr(start, end - start));
            start = end + 1;
        }
        sourceRevision_ = changeCounter_;
    }
    if (ImGui::Button("Copy to clipboard")) ImGui::SetClipboardText(sourceText_.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("gen_scene_%s.cpp (read-only preview, regenerated on every change)",
                        toIdentifier(scene_.name).c_str());
    ImGui::BeginChild("source", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    // The clipper only draws the lines that are actually visible, so very long
    // files stay fast.
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(sourceLines_.size()));
    while (clipper.Step())
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) drawHighlightedLine(sourceLines_[row]);
    ImGui::EndChild();
    ImGui::End();
}

void EditorApp::drawConsole() {
    ImGui::Begin("Console");
    ImGui::BeginChild("log");
    for (const std::string& line : log_) ImGui::TextUnformatted(line.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);  // stick to bottom
    ImGui::EndChild();
    ImGui::End();
}

// =============================================================================
// File dialogs
// =============================================================================

void EditorApp::showDialog(DialogKind kind) {
    static const SDL_DialogFileFilter pngFilter[] = {{"PNG images", "png"}};
    static const SDL_DialogFileFilter sceneFilter[] = {{"Scene files", "json"}};
    // The request object tells the callback which app and which dialog it belongs to.
    auto* request = new DialogRequest{this, kind};
    switch (kind) {
        case DialogKind::ImportPng:
            SDL_ShowOpenFileDialog(onDialogResult, request, window_, pngFilter, 1, nullptr, false);
            break;
        case DialogKind::OpenScene:
            SDL_ShowOpenFileDialog(onDialogResult, request, window_, sceneFilter, 1, nullptr, false);
            break;
        case DialogKind::SaveScene:
            SDL_ShowSaveFileDialog(onDialogResult, request, window_, sceneFilter, 1, nullptr);
            break;
    }
}

void SDLCALL EditorApp::onDialogResult(void* userdata, const char* const* files, int /*filter*/) {
    auto* request = static_cast<DialogRequest*>(userdata);
    if (files && files[0]) {
        std::lock_guard<std::mutex> lock(request->app->dialogMutex_);
        request->app->dialogResults_.push_back({request->kind, files[0]});
    }
    delete request;
    // Wake the main loop, which may be sleeping while waiting for input.
    SDL_Event wake{};
    wake.type = SDL_EVENT_USER;
    SDL_PushEvent(&wake);
}

void EditorApp::processDialogResults() {
    std::vector<DialogResult> results;
    {
        std::lock_guard<std::mutex> lock(dialogMutex_);
        results.swap(dialogResults_);
    }
    for (const DialogResult& result : results) {
        switch (result.kind) {
            case DialogKind::ImportPng: importAsset(result.path); break;
            case DialogKind::OpenScene: openScene(result.path); break;
            case DialogKind::SaveScene: {
                std::string path = result.path;
                if (std::filesystem::path(path).extension() != ".json") path += ".json";
                saveScene(path);
                break;
            }
        }
    }
}
