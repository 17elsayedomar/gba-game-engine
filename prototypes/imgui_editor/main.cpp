// =============================================================================
// Dear ImGui + SDL3 editor prototype — program entry and main loop.
//
// SDL3 gives us a window, keyboard/mouse events and a simple GPU renderer.
// Dear ImGui draws the editor UI on top of it. Everything about the scene
// itself comes from proto_core.
//
// Command-line options (used for the measurements in docs/TESTING.md):
//   --open <scene.json>      open a scene at startup
//   --startup-probe          quit right after the first frame is on screen and
//                            print how long that took
//   --benchmark-drag <secs>  move the first object in circles for <secs> seconds
//                            (simulated drag through the normal command path)
// =============================================================================

#include "editor_app.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // lets SDL provide the right main() on every platform

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char* argv[]) {
    const auto startTime = std::chrono::steady_clock::now();

    std::string openPath;
    bool startupProbe = false;
    double benchmarkSeconds = 0.0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--open" && i + 1 < argc) openPath = argv[++i];
        else if (arg == "--startup-probe") startupProbe = true;
        else if (arg == "--benchmark-drag" && i + 1 < argc) benchmarkSeconds = std::atof(argv[++i]);
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // HIGH_PIXEL_DENSITY: draw at the screen's real resolution instead of letting
    // Windows stretch a blurry low-resolution picture on a 125%/150% display.
    SDL_Window* window = SDL_CreateWindow("GBA Game Engine - Dear ImGui prototype", 1280, 800,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED |
                                              SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "Window/renderer creation failed: %s\n", SDL_GetError());
        return 1;
    }

    // Windows' display scaling (e.g. 150%). Qt applies it automatically; for ImGui
    // we apply it ourselves so text is the same physical size in both prototypes.
    float uiScale = SDL_GetWindowDisplayScale(window);
    if (uiScale <= 0.0f) uiScale = 1.0f;
    SDL_Log("Display scale: %.2f", uiScale);
    // VSync: present at most once per screen refresh (60 Hz) instead of as fast
    // as possible, which would waste CPU and GPU.
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    // ImGui's built-in font is a tiny pixel font. Use Windows' UI font (the one Qt
    // uses) when it exists, so both prototypes are equally readable.
    if (SDL_GetPathInfo("C:/Windows/Fonts/segoeui.ttf", nullptr))
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 15.0f);
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(uiScale);
    style.FontScaleDpi = uiScale;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    {
        EditorApp app(window, renderer);
        if (!openPath.empty()) app.openScene(openPath);
        if (benchmarkSeconds > 0) app.selectFirstObject();

        bool running = true;
        int frame = 0;
        int quietFrames = 0;
        const auto benchmarkStart = std::chrono::steady_clock::now();

        while (running) {
            // --- Power saving -------------------------------------------------
            // A game redraws 60 times per second forever; an editor shouldn't.
            // After a few frames without any input, sleep until the next event
            // (or at most 500 ms). ImGui needs a couple of extra frames after
            // input to finish animations like hover highlights, hence "3".
            SDL_Event event;
            bool haveEvent;
            if (quietFrames >= 3 && benchmarkSeconds <= 0) haveEvent = SDL_WaitEventTimeout(&event, 500);
            else haveEvent = SDL_PollEvent(&event);
            bool anyEvent = false;
            while (haveEvent) {
                anyEvent = true;
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT) running = false;
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                    event.window.windowID == SDL_GetWindowID(window))
                    running = false;
                haveEvent = SDL_PollEvent(&event);
            }
            quietFrames = anyEvent ? 0 : quietFrames + 1;

            // --- Build and draw one frame ------------------------------------
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            if (benchmarkSeconds > 0) app.benchmarkStep(frame);
            app.drawFrame();
            ImGui::Render();
            SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);
            ++frame;

            if (app.quitRequested()) running = false;
            if (startupProbe && frame == 2) {  // frame 1 lays out docking; frame 2 is the real UI
                double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();
                std::printf("startup_ms=%.1f\n", ms);
                running = false;
            }
            if (benchmarkSeconds > 0) {
                double elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - benchmarkStart).count();
                if (elapsed >= benchmarkSeconds) {
                    std::printf("benchmark_frames=%d fps=%.1f\n", frame, frame / elapsed);
                    running = false;
                }
            }
        }
    }  // EditorApp is destroyed here, while the renderer still exists (it owns textures)

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
