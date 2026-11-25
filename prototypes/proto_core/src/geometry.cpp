#include "proto_core/geometry.hpp"

#include <algorithm>
#include <cstdlib>

#include "proto_core/components.hpp"

namespace proto {

Point objectSize(const Scene& scene, const GameObject& object) {
    if (const Component* sprite = object.findComponent(sprite_renderer::kType)) {
        if (const Asset* asset = scene.findAsset(sprite->values[sprite_renderer::kSprite])) {
            if (asset->original.width > 0) return {asset->original.width, asset->original.height};
        }
    }
    return {16, 16};
}

ObjectId pickObject(const Scene& scene, int gbaX, int gbaY) {
    ObjectId best = 0;
    int bestZ = 0;
    for (const GameObject& object : scene.objects) {
        Point size = objectSize(scene, object);
        bool inside = gbaX >= object.x && gbaX < object.x + size.x && gbaY >= object.y &&
                      gbaY < object.y + size.y;
        if (inside && (best == 0 || object.z >= bestZ)) {
            best = object.id;
            bestZ = object.z;
        }
    }
    return best;
}

Point isoToScreen(int tileX, int tileY, int height, const IsoProjection& p) {
    return {(tileX - tileY) * p.tileWidth / 2,
            (tileX + tileY) * p.tileHeight / 2 - height * p.blockHeight};
}

IsoBlockFaces isoBlockFaces(const IsoMap& map, int tileX, int tileY, const IsoProjection& p) {
    const int h = map.heightAt(tileX, tileY);
    const int halfW = p.tileWidth / 2;
    const int halfH = p.tileHeight / 2;
    const int wall = h * p.blockHeight;  // how tall the side walls are on screen

    // The diamond's four corners at the top of the column.
    Point top = isoToScreen(tileX, tileY, h, p);
    Point right{top.x + halfW, top.y + halfH};
    Point bottom{top.x, top.y + p.tileHeight};
    Point left{top.x - halfW, top.y + halfH};

    IsoBlockFaces faces;
    faces.top = {top, right, bottom, left};
    // Each wall hangs down from two diamond edges to the ground.
    faces.left = {left, bottom, Point{bottom.x, bottom.y + wall}, Point{left.x, left.y + wall}};
    faces.right = {bottom, right, Point{right.x, right.y + wall}, Point{bottom.x, bottom.y + wall}};
    return faces;
}

std::vector<Point> isoDrawOrder(const IsoMap& map) {
    std::vector<Point> order;
    for (int y = 0; y < map.depth; ++y)
        for (int x = 0; x < map.width; ++x) order.push_back({x, y});
    // Smaller x + y is farther away (higher on screen) and must be drawn first.
    // stable_sort keeps a fixed order for ties, so drawing is deterministic.
    std::stable_sort(order.begin(), order.end(),
                     [](Point a, Point b) { return a.x + a.y < b.x + b.y; });
    return order;
}

// Is `p` inside the diamond whose top corner is `top`? A diamond is the set of
// points where |dx| / halfWidth + |dy| / halfHeight <= 1, measured from its center.
// Multiplying through by halfWidth * halfHeight keeps everything in integers.
static bool insideDiamond(Point p, Point top, const IsoProjection& proj) {
    const int halfW = proj.tileWidth / 2;
    const int halfH = proj.tileHeight / 2;
    const int dx = std::abs(p.x - top.x);
    const int dy = std::abs(p.y - (top.y + halfH));
    return dx * halfH + dy * halfW <= halfW * halfH;
}

std::optional<Point> isoPickTile(const IsoMap& map, Point screen, const IsoProjection& proj) {
    std::vector<Point> order = isoDrawOrder(map);
    // Walk front-to-back: the first top face that contains the point is the one
    // the user sees (anything behind it is covered).
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        Point top = isoToScreen(it->x, it->y, map.heightAt(it->x, it->y), proj);
        if (insideDiamond(screen, top, proj)) return *it;
    }
    return std::nullopt;
}

int nodeHeight(const GraphNode& node) {
    int rows = static_cast<int>(std::max(node.inputs.size(), node.outputs.size()));
    return NodeLayout::kHeaderHeight + std::max(rows, 1) * NodeLayout::kPinRowHeight;
}

Point pinPosition(const GraphNode& node, bool isOutput, int pinIndex) {
    int y = node.y + NodeLayout::kHeaderHeight + pinIndex * NodeLayout::kPinRowHeight +
            NodeLayout::kPinRowHeight / 2;
    int x = isOutput ? node.x + NodeLayout::kWidth : node.x;
    return {x, y};
}

std::array<Point, 4> linkCurve(Point from, Point to) {
    int pull = std::max(40, std::abs(to.x - from.x) / 2);
    return {from, Point{from.x + pull, from.y}, Point{to.x - pull, to.y}, to};
}

bool hitTestPin(const NodeGraph& graph, Point p, int& nodeId, bool& isOutput, int& pinIndex) {
    const int r = NodeLayout::kPinRadius * 2;
    for (const GraphNode& node : graph.nodes) {
        for (int pass = 0; pass < 2; ++pass) {
            const bool output = pass == 1;
            const auto& pins = output ? node.outputs : node.inputs;
            for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
                Point c = pinPosition(node, output, i);
                if ((p.x - c.x) * (p.x - c.x) + (p.y - c.y) * (p.y - c.y) <= r * r) {
                    nodeId = node.id;
                    isOutput = output;
                    pinIndex = i;
                    return true;
                }
            }
        }
    }
    return false;
}

bool connectPins(NodeGraph& graph, int fromNode, int fromPin, int toNode, int toPin) {
    if (fromNode == toNode) return false;
    disconnectInput(graph, toNode, toPin);
    graph.links.push_back({fromNode, fromPin, toNode, toPin});
    return true;
}

bool disconnectInput(NodeGraph& graph, int toNode, int toPin) {
    auto before = graph.links.size();
    graph.links.erase(std::remove_if(graph.links.begin(), graph.links.end(),
                                     [&](const GraphLink& l) { return l.toNode == toNode && l.toPin == toPin; }),
                      graph.links.end());
    return graph.links.size() != before;
}

}  // namespace proto
