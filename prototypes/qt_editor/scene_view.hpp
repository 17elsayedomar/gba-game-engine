#pragma once
// =============================================================================
// SceneView — the 2D viewport, built on QGraphicsView.
//
// QGraphicsView is Qt's ready-made 2D canvas: you add "items" (pictures,
// rectangles, text) to a QGraphicsScene and Qt handles drawing, zooming,
// scrolling and finding which item is under the mouse.
//
// Coordinates: one QGraphicsScene unit = one GBA pixel, so (0, 0)..(240, 160)
// is exactly the GBA screen. Zoom is just view->setTransform(scale).
//
// The proto_core scene stays the single source of truth: dragging does NOT move
// the Qt item directly. It sends a SetPositionCommand, and the item is moved
// afterwards when sync() copies positions from the model.
// =============================================================================

#include "proto_core/model.hpp"

#include <QGraphicsView>

#include <map>

class EditorContext;
class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;

class SceneView : public QGraphicsView {
    Q_OBJECT
public:
    explicit SceneView(EditorContext* context, QWidget* parent = nullptr);

    void sync();  // copy every object's position/sprite from the model into Qt items
    void setZoom(int zoom);
    void setSnapToGrid(bool snap);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    QPoint toGba(const QPoint& viewPos) const;

    // What Qt draws for one GameObject: its sprite, or a placeholder box.
    struct ObjectItems {
        QGraphicsPixmapItem* sprite = nullptr;
        QGraphicsRectItem* box = nullptr;
    };

    EditorContext* context_;
    QGraphicsScene* graphicsScene_;
    std::map<proto::ObjectId, ObjectItems> items_;
    QGraphicsRectItem* selectionOutline_;
    QGraphicsSimpleTextItem* selectionLabel_;
    bool snap_ = false;
    bool dragging_ = false;
    int dragSession_ = 0;
    QPoint dragOffset_;
};
