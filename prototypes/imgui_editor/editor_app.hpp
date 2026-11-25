#pragma once
// =============================================================================
// EditorApp — the Dear ImGui prototype's whole user interface.
//
// HOW IMMEDIATE-MODE UI WORKS (the key idea of this prototype):
// Every frame (about 60 times a second while you interact), drawFrame() runs
// from top to bottom and *describes* every panel again: "there is a button
// here", "there is a number field showing object.x here". ImGui answers
// immediately — e.g. Button() returns true on the frame it was clicked.
//
// Because the UI is rebuilt from the scene every frame, there is no second copy
// of the data inside the UI to keep in sync. After an undo, the next frame simply
// shows the new values. Compare this with the Qt prototype, which must tell each
// widget to refresh itself.
//
// The model, commands, save/load and code generation all come from proto_core;
// this class only draws and turns mouse/keyboard input into proto_core commands.
// =============================================================================

#include "proto_core/commands.hpp"
#include "proto_core/model.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class EditorApp {
public:
    EditorApp(SDL_Window* window, SDL_Renderer* renderer);
    ~EditorApp();

    // Describes the entire UI for one frame. Call between ImGui::NewFrame() and ImGui::Render().
    void drawFrame();

    bool openScene(const std::string& path);
    void selectFirstObject();
    bool quitRequested() const { return quitRequested_; }

    // Used by --benchmark-drag: moves the selected object one step along a circle,
    // through exactly the same command path a mouse drag uses.
    void benchmarkStep(int frameNumber);

private:
    // ---- panels (one function per dockable window) ----
    void drawMenuBar();
    void drawHierarchy();
    void drawInspector();
    void drawAssets();
    void drawViewport();
    void drawIsoGrid();
    void drawNodeGraph();
    void drawSourcePanel();
    void drawConsole();
    void handleShortcuts();
    void buildDefaultLayout(ImGuiID dockspaceId);

    // ---- actions ----
    void execute(std::unique_ptr<proto::Command> command);
    void undo();
    void redo();
    void newScene();
    void saveScene(const std::string& path);
    void importAsset(const std::string& path);
    void createObject();
    void deleteSelected();
    void log(const std::string& message);
    void markChanged() { ++changeCounter_; }

    // ---- helpers ----
    SDL_Texture* textureFor(const proto::Asset& asset);
    void clearTextures();
    void acceptAssetDropOnObject(proto::ObjectId target);

    // ---- native file dialogs (SDL3) ----
    // SDL may call the dialog callback on another thread, so results are put in a
    // locked list and handled at the start of the next frame on the main thread.
    enum class DialogKind { ImportPng, OpenScene, SaveScene };
    struct DialogResult {
        DialogKind kind;
        std::string path;
    };
    void showDialog(DialogKind kind);
    static void SDLCALL onDialogResult(void* userdata, const char* const* files, int filter);
    void processDialogResults();
    std::mutex dialogMutex_;
    std::vector<DialogResult> dialogResults_;
    struct DialogRequest {
        EditorApp* app;
        DialogKind kind;
    };

    SDL_Window* window_;
    SDL_Renderer* renderer_;

    proto::Scene scene_;
    proto::CommandHistory history_;
    int changeCounter_ = 0;  // bumps on every change; the source panel watches it
    std::string scenePath_;
    std::vector<std::string> log_;
    bool quitRequested_ = false;
    bool resetLayout_ = false;

    // Selection is UI state, not part of the scene: it isn't saved and isn't undone.
    proto::ObjectId selected_ = 0;

    // A "session" groups many small edits into one undo step (see commands.hpp).
    int nextSession_ = 1;
    int positionSession_ = 0;
    int propertySession_ = 0;

    // Inspector rename field.
    std::string renameBuffer_;
    proto::ObjectId renameTarget_ = 0;
    bool renameActive_ = false;

    // Scene viewport.
    float zoom_ = 2.0f;
    bool snapToGrid_ = false;
    bool draggingObject_ = false;
    int dragOffsetX_ = 0;
    int dragOffsetY_ = 0;
    int benchmarkSession_ = 0;

    // Node graph.
    int draggingNode_ = 0;
    ImVec2 nodeDragOffset_{0, 0};
    bool linking_ = false;
    int linkFromNode_ = 0;
    int linkFromPin_ = 0;

    // Generated source cache (regenerated only when the scene changes).
    int sourceRevision_ = -1;
    std::vector<std::string> sourceLines_;
    std::string sourceText_;

    // One GPU texture per asset, created the first time it is drawn.
    std::map<proto::AssetId, SDL_Texture*> textures_;
};
