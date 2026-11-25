// =============================================================================
// Qt 6 Widgets editor prototype — program entry.
//
// Unlike the ImGui prototype there is no hand-written main loop: app.exec()
// runs Qt's event loop, which sleeps until something happens (a click, a key,
// a timer) and then calls the right widget's event handler.
//
// Command-line options (same as the ImGui prototype, see docs/TESTING.md):
//   --open <scene.json>      open a scene at startup
//   --startup-probe          quit as soon as the window has been painted once
//   --benchmark-drag <secs>  move the first object 60 times a second for <secs>
//   --opengl                 render the Scene viewport through OpenGL
// =============================================================================

#include "editor_context.hpp"
#include "main_window.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QStyleFactory>
#include <QTimer>

#include <chrono>
#include <cstdio>

int main(int argc, char* argv[]) {
    const auto startTime = std::chrono::steady_clock::now();
    QApplication app(argc, argv);

    QString openPath;
    bool startupProbe = false, openGL = false;
    double benchmarkSeconds = 0.0;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--open" && i + 1 < args.size()) openPath = args[++i];
        else if (args[i] == "--startup-probe") startupProbe = true;
        else if (args[i] == "--benchmark-drag" && i + 1 < args.size()) benchmarkSeconds = args[++i].toDouble();
        else if (args[i] == "--opengl") openGL = true;
    }

    // "Fusion" is Qt's own cross-platform look; its dark palette roughly matches
    // the ImGui prototype so screenshots are comparable.
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(37, 37, 42));
    dark.setColor(QPalette::WindowText, QColor(220, 220, 220));
    dark.setColor(QPalette::Base, QColor(28, 28, 32));
    dark.setColor(QPalette::AlternateBase, QColor(45, 45, 52));
    dark.setColor(QPalette::Text, QColor(220, 220, 220));
    dark.setColor(QPalette::Button, QColor(50, 52, 60));
    dark.setColor(QPalette::ButtonText, QColor(220, 220, 220));
    dark.setColor(QPalette::Highlight, QColor(66, 110, 180));
    dark.setColor(QPalette::HighlightedText, Qt::white);
    dark.setColor(QPalette::ToolTipBase, QColor(50, 52, 60));
    dark.setColor(QPalette::ToolTipText, QColor(220, 220, 220));
    dark.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
    dark.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
    dark.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
    app.setPalette(dark);

    EditorContext context;
    MainWindow window(&context);
    if (openGL) window.useOpenGLViewport();
    if (!openPath.isEmpty()) window.openScene(openPath);
    window.showMaximized();

    if (startupProbe) {
        // Runs once the event loop has started, i.e. after the window was shown.
        // repaint() forces every widget to paint right now, so the time printed
        // includes the first complete picture on screen.
        QTimer::singleShot(0, &window, [&] {
            window.repaint();
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();
            std::printf("startup_ms=%.1f\n", ms);
            std::fflush(stdout);
            app.quit();
        });
    }

    QTimer benchmarkTimer;
    QElapsedTimer benchmarkClock;
    int benchmarkSteps = 0;
    if (benchmarkSeconds > 0) {
        // ImGui moves the object once per frame (60 per second with VSync), so
        // Qt gets the same workload: one move every ~16.7 ms.
        window.selectFirstObject();
        benchmarkTimer.setTimerType(Qt::PreciseTimer);
        benchmarkTimer.setInterval(16);
        QObject::connect(&benchmarkTimer, &QTimer::timeout, &window, [&] {
            window.benchmarkStep(benchmarkSteps++);
            if (benchmarkClock.elapsed() >= benchmarkSeconds * 1000) {
                std::printf("benchmark_steps=%d rate=%.1f/s\n", benchmarkSteps,
                            benchmarkSteps * 1000.0 / benchmarkClock.elapsed());
                std::fflush(stdout);
                app.quit();
            }
        });
        benchmarkClock.start();
        benchmarkTimer.start();
    }

    return app.exec();
}
