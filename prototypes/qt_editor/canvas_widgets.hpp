#pragma once
// =============================================================================
// Two custom-drawn panels: the isometric tile grid and the node graph.
//
// Both are plain QWidgets that draw themselves with QPainter in paintEvent().
// Qt calls paintEvent() only when something asked for a redraw (update()), not
// every frame. All shapes come from proto_core/geometry, so these panels show
// exactly the same picture as the ImGui prototype.
//
// (For a big node editor, QGraphicsView would give zoom/scroll and hit-testing
// for free, like the Scene viewport. A QWidget keeps this one short and makes
// the comparison with ImGui's draw-list code direct.)
// =============================================================================

#include "proto_core/geometry.hpp"

#include <QWidget>

#include <optional>

class EditorContext;

class IsoGridWidget : public QWidget {
    Q_OBJECT
public:
    explicit IsoGridWidget(EditorContext* context, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    std::optional<proto::Point> tileAt(const QPointF& pos) const;
    QPointF anchor() const;

    EditorContext* context_;
    proto::IsoProjection projection_;
    std::optional<proto::Point> hovered_;
    static constexpr double kScale = 2.0;  // each GBA pixel drawn as 2x2
};

class NodeGraphWidget : public QWidget {
    Q_OBJECT
public:
    explicit NodeGraphWidget(EditorContext* context, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    proto::Point toGraph(const QPointF& pos) const;

    EditorContext* context_;
    int draggingNode_ = 0;
    proto::Point dragOffset_;
    bool linking_ = false;
    int linkFromNode_ = 0;
    int linkFromPin_ = 0;
    proto::Point mouse_;
};
