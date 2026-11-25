#include "canvas_widgets.hpp"

#include "editor_context.hpp"

#include "proto_core/commands.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

using namespace proto;

// =============================================================================
// Isometric grid (feature 13)
// =============================================================================

IsoGridWidget::IsoGridWidget(EditorContext* context, QWidget* parent) : QWidget(parent), context_(context) {
    setMouseTracking(true);  // get mouse-move events even with no button held (for hover)
    setMinimumSize(200, 200);
    connect(context_, &EditorContext::sceneChanged, this, qOverload<>(&QWidget::update));
}

QPointF IsoGridWidget::anchor() const { return {width() * 0.5, 80.0}; }  // where tile (0,0) sits

std::optional<Point> IsoGridWidget::tileAt(const QPointF& pos) const {
    QPointF p = (pos - anchor()) / kScale;
    return isoPickTile(context_->scene().iso,
                       {static_cast<int>(std::floor(p.x())), static_cast<int>(std::floor(p.y()))}, projection_);
}

void IsoGridWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(36, 36, 42));
    painter.setPen(QColor(220, 220, 220));
    painter.drawText(QRect(8, 4, width() - 16, 40), Qt::TextWordWrap,
                     "Left-click a tile to raise it, right-click to lower it (undoable). "
                     "Projection and picking come from proto_core/geometry.");

    const IsoMap& map = context_->scene().iso;
    auto polygon = [&](const std::array<Point, 4>& points) {
        QPolygonF result;
        for (Point p : points) result << anchor() + QPointF(p.x, p.y) * kScale;
        return result;
    };
    for (Point tile : isoDrawOrder(map)) {  // back to front
        IsoBlockFaces faces = isoBlockFaces(map, tile.x, tile.y, projection_);
        int h = map.heightAt(tile.x, tile.y);
        int shade = 90 + h * 30;
        painter.setPen(Qt::NoPen);
        if (h > 0) {
            painter.setBrush(QColor(shade * 6 / 10, shade * 5 / 10, 40));
            painter.drawPolygon(polygon(faces.left));
            painter.setBrush(QColor(shade * 4 / 10, shade * 3 / 10, 30));
            painter.drawPolygon(polygon(faces.right));
        }
        bool isHovered = hovered_ && hovered_->x == tile.x && hovered_->y == tile.y;
        painter.setBrush(QColor(60, shade, 70));
        painter.setPen(isHovered ? QPen(QColor(255, 230, 80), 2) : QPen(QColor(0, 0, 0, 90), 1));
        painter.drawPolygon(polygon(faces.top));
    }
    if (hovered_) {
        painter.setPen(QColor(230, 230, 230));
        painter.drawText(8, height() - 10,
                         QString("tile (%1, %2), height %3")
                             .arg(hovered_->x).arg(hovered_->y).arg(map.heightAt(hovered_->x, hovered_->y)));
    }
}

void IsoGridWidget::mousePressEvent(QMouseEvent* event) {
    std::optional<Point> tile = tileAt(event->position());
    if (!tile) return;
    int h = context_->scene().iso.heightAt(tile->x, tile->y);
    if (event->button() == Qt::LeftButton)
        context_->execute(std::make_unique<SetIsoHeightCommand>(tile->x, tile->y, h + 1));
    else if (event->button() == Qt::RightButton)
        context_->execute(std::make_unique<SetIsoHeightCommand>(tile->x, tile->y, h - 1));
}

void IsoGridWidget::mouseMoveEvent(QMouseEvent* event) {
    std::optional<Point> tile = tileAt(event->position());
    bool changed = tile.has_value() != hovered_.has_value() ||
                   (tile && (tile->x != hovered_->x || tile->y != hovered_->y));
    hovered_ = tile;
    if (changed) update();  // redraw only when the highlighted tile changes
}

void IsoGridWidget::leaveEvent(QEvent*) {
    hovered_.reset();
    update();
}

// =============================================================================
// Node graph (feature 14)
// =============================================================================

NodeGraphWidget::NodeGraphWidget(EditorContext* context, QWidget* parent) : QWidget(parent), context_(context) {
    setMinimumSize(200, 200);
    connect(context_, &EditorContext::sceneChanged, this, qOverload<>(&QWidget::update));
}

Point NodeGraphWidget::toGraph(const QPointF& pos) const {
    const int top = 44;  // room for the help text
    return {static_cast<int>(pos.x()), static_cast<int>(pos.y()) - top};
}

void NodeGraphWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(30, 30, 36));
    painter.setPen(QColor(255, 255, 255, 12));
    for (int x = 0; x < width(); x += 32) painter.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += 32) painter.drawLine(0, y, width(), y);
    painter.setPen(QColor(220, 220, 220));
    painter.drawText(QRect(8, 4, width() - 16, 40), Qt::TextWordWrap,
                     "Drag a node by its title. Drag from an output pin (right) to an input pin (left) "
                     "to connect. Drag a wire off an input to disconnect it.");
    painter.translate(0, 44);  // graph coordinates start below the help text

    const NodeGraph& graph = context_->scene().graph;
    auto drawWire = [&](Point from, Point to, QColor color) {
        auto c = linkCurve(from, to);
        QPainterPath path(QPointF(c[0].x, c[0].y));
        path.cubicTo(c[1].x, c[1].y, c[2].x, c[2].y, c[3].x, c[3].y);
        painter.setPen(QPen(color, 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    };
    for (const GraphLink& link : graph.links) {
        const GraphNode* from = graph.findNode(link.fromNode);
        const GraphNode* to = graph.findNode(link.toNode);
        if (from && to)
            drawWire(pinPosition(*from, true, link.fromPin), pinPosition(*to, false, link.toPin), QColor(120, 200, 255));
    }
    if (linking_) {
        if (const GraphNode* from = graph.findNode(linkFromNode_))
            drawWire(pinPosition(*from, true, linkFromPin_), mouse_, QColor(255, 230, 80));
    }

    QFontMetrics metrics(font());
    for (const GraphNode& node : graph.nodes) {
        QRectF box(node.x, node.y, NodeLayout::kWidth, nodeHeight(node));
        painter.setPen(QColor(0, 0, 0, 200));
        painter.setBrush(QColor(55, 58, 70, 245));
        painter.drawRoundedRect(box, 6, 6);
        // Header: a rounded rect clipped to the top part.
        painter.save();
        painter.setClipRect(QRectF(node.x, node.y, NodeLayout::kWidth, NodeLayout::kHeaderHeight));
        painter.setBrush(QColor(80, 90, 140));
        painter.drawRoundedRect(box, 6, 6);
        painter.restore();
        painter.setPen(Qt::white);
        painter.drawText(QRectF(node.x + 8, node.y, NodeLayout::kWidth - 16, NodeLayout::kHeaderHeight),
                         Qt::AlignVCenter, QString::fromStdString(node.title));
        for (int pass = 0; pass < 2; ++pass) {
            const bool output = pass == 1;
            const auto& pins = output ? node.outputs : node.inputs;
            for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
                Point c = pinPosition(node, output, i);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(120, 200, 255));
                painter.drawEllipse(QPointF(c.x, c.y), NodeLayout::kPinRadius, NodeLayout::kPinRadius);
                QString text = QString::fromStdString(pins[i]);
                int textX = output ? c.x - 12 - metrics.horizontalAdvance(text) : c.x + 12;
                painter.setPen(QColor(220, 220, 220));
                painter.drawText(textX, c.y + metrics.ascent() / 2 - 1, text);
            }
        }
    }
}

void NodeGraphWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    NodeGraph& graph = context_->scene().graph;
    Point p = toGraph(event->position());
    mouse_ = p;
    int nodeId = 0, pin = 0;
    bool isOutput = false;
    if (hitTestPin(graph, p, nodeId, isOutput, pin)) {
        if (isOutput) {
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
            if (disconnectInput(graph, nodeId, pin)) context_->notifyChanged();
        }
        return;
    }
    for (const GraphNode& node : graph.nodes) {  // the last match is the topmost node
        if (p.x >= node.x && p.x < node.x + NodeLayout::kWidth && p.y >= node.y && p.y < node.y + nodeHeight(node)) {
            draggingNode_ = node.id;
            dragOffset_ = {p.x - node.x, p.y - node.y};
        }
    }
}

void NodeGraphWidget::mouseMoveEvent(QMouseEvent* event) {
    mouse_ = toGraph(event->position());
    if (GraphNode* node = context_->scene().graph.findNode(draggingNode_)) {
        node->x = mouse_.x - dragOffset_.x;
        node->y = mouse_.y - dragOffset_.y;
    }
    if (draggingNode_ != 0 || linking_) update();
}

void NodeGraphWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (linking_) {
        NodeGraph& graph = context_->scene().graph;
        int nodeId = 0, pin = 0;
        bool isOutput = true;
        if (hitTestPin(graph, toGraph(event->position()), nodeId, isOutput, pin) && !isOutput &&
            connectPins(graph, linkFromNode_, linkFromPin_, nodeId, pin))
            context_->notifyChanged();
    }
    linking_ = false;
    draggingNode_ = 0;
    update();
}
