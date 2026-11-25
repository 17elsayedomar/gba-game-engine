#pragma once
// =============================================================================
// proto_core/geometry.hpp — shared drawing math for the isometric grid and the
// node graph. The UIs only fill the polygons/curves this code calculates, so
// both prototypes show exactly the same picture and pick the same tile.
// =============================================================================

#include "proto_core/model.hpp"

#include <array>
#include <optional>
#include <vector>

namespace proto {

struct Point {
    int x = 0;
    int y = 0;
};

// ----------------------------------------------------------------------------
// 2D scene viewport
// ----------------------------------------------------------------------------

// Size in GBA pixels of an object in the viewport: its sprite's size, or a
// 16x16 placeholder box when it has no sprite.
Point objectSize(const Scene& scene, const GameObject& object);

// The object under a GBA-pixel position, or 0. The object drawn on top wins:
// highest z; for equal z, the one later in the list (it is drawn later).
ObjectId pickObject(const Scene& scene, int gbaX, int gbaY);

// ----------------------------------------------------------------------------
// Isometric projection
// ----------------------------------------------------------------------------
//
// Isometric view = looking at a grid of square floor tiles from above and at an
// angle, so each square appears as a diamond. Moving +1 tile in X goes down-right
// on screen; moving +1 tile in Y goes down-left; going up 1 block of height moves
// straight up on screen.
//
//              (0,0)
//             /     \        screenX = (tileX - tileY) * tileWidth  / 2
//          (0,1)   (1,0)     screenY = (tileX + tileY) * tileHeight / 2
//             \     /                  - height * blockHeight
//              (1,1)
//
// The 2:1 diamond (32x16 pixels) is the classic "pixel-art isometric" ratio
// (technically "dimetric"), because its edges are clean 2-pixels-across,
// 1-pixel-down staircases.
struct IsoProjection {
    int tileWidth = 32;
    int tileHeight = 16;
    int blockHeight = 8;  // screen pixels per height step
};

// Screen position of the TOP corner of a tile's diamond, at the given height,
// relative to the screen position of tile (0, 0)'s top corner at height 0.
Point isoToScreen(int tileX, int tileY, int height, const IsoProjection& projection);

// The three visible faces of one block column, each a 4-point polygon, in
// screen coordinates. `top` is always valid; `left`/`right` are the side walls
// (zero-height when the tile is flat).
struct IsoBlockFaces {
    std::array<Point, 4> top;
    std::array<Point, 4> left;
    std::array<Point, 4> right;
};
IsoBlockFaces isoBlockFaces(const IsoMap& map, int tileX, int tileY, const IsoProjection& projection);

// Tiles in back-to-front order (the order to draw them so nearer blocks cover
// farther ones). For a single-height column grid, sorting by tileX + tileY is
// enough. Overlapping heights, bridges and moving objects need the more robust
// sorting discussed in docs/ARCHITECTURE.md section 9.
std::vector<Point> isoDrawOrder(const IsoMap& map);

// Which tile's TOP face is under a screen point? Tests front tiles first, so the
// tile you can actually see wins. Returns nullopt if the point hits no top face.
std::optional<Point> isoPickTile(const IsoMap& map, Point screen, const IsoProjection& projection);

// ----------------------------------------------------------------------------
// Node graph layout
// ----------------------------------------------------------------------------

struct NodeLayout {
    static constexpr int kWidth = 190;
    static constexpr int kHeaderHeight = 26;
    static constexpr int kPinRowHeight = 24;
    static constexpr int kPinRadius = 6;
};

int nodeHeight(const GraphNode& node);
// Center of a pin, in graph coordinates. Output pins sit on the right edge,
// input pins on the left edge.
Point pinPosition(const GraphNode& node, bool isOutput, int pinIndex);

// The four control points of the S-shaped curve (a cubic Bezier) drawn for a link:
// start, two "pull" points that make it leave/enter horizontally, and end.
std::array<Point, 4> linkCurve(Point from, Point to);

// Finds a pin within kPinRadius*2 of `p`. Returns true and fills the outputs if found.
bool hitTestPin(const NodeGraph& graph, Point p, int& nodeId, bool& isOutput, int& pinIndex);

// Connects an output to an input. An input accepts only one wire, so any existing
// wire into that input is replaced. Connecting a node to itself is refused.
bool connectPins(NodeGraph& graph, int fromNode, int fromPin, int toNode, int toPin);

// Removes every wire going into the given input pin. Returns true if any was removed.
bool disconnectInput(NodeGraph& graph, int toNode, int toPin);

}  // namespace proto
