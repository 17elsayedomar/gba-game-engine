#include "scene_view.hpp"

#include "editor_context.hpp"
#include "scene_tree_model.hpp"  // kAssetMimeType

#include "proto_core/components.hpp"
#include "proto_core/geometry.hpp"

#include <QDragEnterEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QMimeData>
#include <QMouseEvent>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>

using namespace proto;

static constexpr int kGbaWidth = 240;
static constexpr int kGbaHeight = 160;

SceneView::SceneView(EditorContext* context, QWidget* parent)
    : QGraphicsView(parent), context_(context), graphicsScene_(new QGraphicsScene(this)) {
    setScene(graphicsScene_);
    setAcceptDrops(true);
    setBackgroundBrush(QColor(45, 45, 52));
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    // A margin around the GBA screen so objects partly off-screen are still visible.
    graphicsScene_->setSceneRect(-16, -16, kGbaWidth + 32, kGbaHeight + 40);

    // The GBA screen: a dark rectangle behind everything...
    auto* screen = graphicsScene_->addRect(0, 0, kGbaWidth, kGbaHeight, Qt::NoPen, QColor(24, 28, 38));
    screen->setZValue(-1000);

    // ...and a yellow border in front of everything. A "cosmetic" pen stays 2
    // screen pixels wide at every zoom level.
    QPen borderPen(QColor(255, 230, 80), 2);
    borderPen.setCosmetic(true);
    auto* border = graphicsScene_->addRect(-0.5, -0.5, kGbaWidth + 1, kGbaHeight + 1, borderPen);
    border->setZValue(1000);
    auto* label = graphicsScene_->addSimpleText("GBA screen 240x160 (pixels outside are off-screen)");
    label->setBrush(QColor(255, 230, 80));
    label->setFlag(QGraphicsItem::ItemIgnoresTransformations);  // text keeps its size when zooming
    label->setPos(0, kGbaHeight + 3);

    QPen selectionPen(QColor(255, 200, 0), 2);
    selectionPen.setCosmetic(true);
    selectionOutline_ = graphicsScene_->addRect(0, 0, 1, 1, selectionPen);
    selectionOutline_->setZValue(999);
    selectionLabel_ = graphicsScene_->addSimpleText("");
    selectionLabel_->setBrush(QColor(255, 200, 0));
    selectionLabel_->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    selectionLabel_->setZValue(999);

    setZoom(2);
    connect(context_, &EditorContext::sceneChanged, this, &SceneView::sync);
    connect(context_, &EditorContext::selectionChanged, this, &SceneView::sync);
    sync();
}

void SceneView::setZoom(int zoom) {
    // Qt measures in logical pixels, so 3x on a 150% display would be 4.5 screen
    // pixels per GBA pixel. Round to a whole number of real screen pixels so every
    // GBA pixel is exactly the same size (the ImGui prototype does the same).
    const double ratio = devicePixelRatioF();
    const double physical = std::max(1.0, std::round(zoom * ratio));
    setTransform(QTransform::fromScale(physical / ratio, physical / ratio));
}

void SceneView::setSnapToGrid(bool snap) {
    snap_ = snap;
    viewport()->update();  // redraw so the grid appears/disappears
}

void SceneView::sync() {
    const Scene& scene = context_->scene();

    // 1. Remove items whose objects no longer exist (deleted, or undone creation).
    std::set<ObjectId> alive;
    for (const GameObject& object : scene.objects) alive.insert(object.id);
    for (auto it = items_.begin(); it != items_.end();) {
        if (!alive.count(it->first)) {
            delete it->second.sprite;
            delete it->second.box;
            it = items_.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Create or update one item per object.
    for (int index = 0; index < static_cast<int>(scene.objects.size()); ++index) {
        const GameObject& object = scene.objects[index];
        ObjectItems& items = items_[object.id];
        if (!items.sprite) {
            items.sprite = graphicsScene_->addPixmap({});
            items.sprite->setTransformationMode(Qt::FastTransformation);  // crisp pixels when zoomed
            items.box = graphicsScene_->addRect(0, 0, 16, 16, QPen(QColor(150, 170, 220), 0),
                                                QColor(90, 110, 160, 120));
        }
        const Component* sprite = object.findComponent(sprite_renderer::kType);
        QPixmap pixmap = sprite ? context_->pixmapFor(sprite->values[sprite_renderer::kSprite]) : QPixmap();
        items.sprite->setPixmap(pixmap);
        items.sprite->setVisible(!pixmap.isNull());
        items.box->setVisible(pixmap.isNull());
        // Higher z on top; for equal z, later objects in the list on top (same as ImGui).
        double drawOrder = object.z + index / 10000.0;
        for (QGraphicsItem* item : {static_cast<QGraphicsItem*>(items.sprite), static_cast<QGraphicsItem*>(items.box)}) {
            item->setPos(object.x, object.y);
            item->setZValue(drawOrder);
        }
    }

    // 3. Selection outline and name label.
    const GameObject* selected = scene.findObject(context_->selected());
    selectionOutline_->setVisible(selected != nullptr);
    selectionLabel_->setVisible(selected != nullptr);
    if (selected) {
        Point size = objectSize(scene, *selected);
        selectionOutline_->setRect(selected->x - 0.5, selected->y - 0.5, size.x + 1, size.y + 1);
        selectionLabel_->setText(QString::fromStdString(selected->name));
        selectionLabel_->setPos(selected->x, selected->y - 6);
    }
}

QPoint SceneView::toGba(const QPoint& viewPos) const {
    QPointF p = mapToScene(viewPos);
    return {static_cast<int>(std::floor(p.x())), static_cast<int>(std::floor(p.y()))};
}

void SceneView::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return QGraphicsView::mousePressEvent(event);
    QPoint gba = toGba(event->position().toPoint());
    ObjectId hit = pickObject(context_->scene(), gba.x(), gba.y());
    context_->select(hit);
    if (const GameObject* object = context_->scene().findObject(hit)) {
        dragging_ = true;
        dragSession_ = context_->newSession();
        dragOffset_ = gba - QPoint(object->x, object->y);
    }
}

void SceneView::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) return QGraphicsView::mouseMoveEvent(event);
    const GameObject* object = context_->scene().findObject(context_->selected());
    if (!object) return;
    QPoint p = toGba(event->position().toPoint()) - dragOffset_;
    int x = p.x(), y = p.y();
    if (snap_) {
        x = static_cast<int>(std::lround(x / 8.0)) * 8;
        y = static_cast<int>(std::lround(y / 8.0)) * 8;
    }
    if (x != object->x || y != object->y)
        context_->execute(std::make_unique<SetPositionCommand>(object->id, x, y, object->z, dragSession_));
}

void SceneView::mouseReleaseEvent(QMouseEvent* event) {
    dragging_ = false;
    QGraphicsView::mouseReleaseEvent(event);
}

// --- Asset drops: onto an object assigns its sprite; onto empty space creates an object.

void SceneView::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(kAssetMimeType)) event->acceptProposedAction();
}

void SceneView::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat(kAssetMimeType)) event->acceptProposedAction();
}

void SceneView::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat(kAssetMimeType)) return;
    AssetId assetId = event->mimeData()->data(kAssetMimeType).toLongLong();
    QPoint gba = toGba(event->position().toPoint());
    ObjectId hit = pickObject(context_->scene(), gba.x(), gba.y());
    if (hit != 0) {
        context_->execute(std::make_unique<AssignSpriteCommand>(hit, assetId));
        context_->select(hit);
    } else if (const Asset* asset = context_->scene().findAsset(assetId)) {
        std::string name = std::filesystem::path(asset->name).stem().string();
        auto command = std::make_unique<CreateObjectCommand>(name, gba.x() - asset->original.width / 2,
                                                             gba.y() - asset->original.height / 2, assetId);
        CreateObjectCommand* raw = command.get();
        context_->execute(std::move(command));
        context_->select(raw->createdId());
    }
    event->acceptProposedAction();
}

void SceneView::drawForeground(QPainter* painter, const QRectF&) {
    if (!snap_) return;
    // 8x8 grid = the GBA's tile size. Drawn in scene units, so it zooms along.
    QPen pen(QColor(255, 255, 255, 25), 0);  // width 0 = always 1 screen pixel
    painter->setPen(pen);
    for (int x = 8; x < kGbaWidth; x += 8) painter->drawLine(QPointF(x, 0), QPointF(x, kGbaHeight));
    for (int y = 8; y < kGbaHeight; y += 8) painter->drawLine(QPointF(0, y), QPointF(kGbaWidth, y));
}
