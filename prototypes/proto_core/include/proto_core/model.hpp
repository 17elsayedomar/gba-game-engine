#pragma once
// =============================================================================
// proto_core/model.hpp — the shared, UI-independent project model.
//
// PROTOTYPE CODE: this exists only to compare editor UI frameworks (decision
// D-003). It is deliberately small and is not the production engine model.
//
// The single most important rule in this file: NO Qt or ImGui types. Both editor
// prototypes include this header, and so could a command-line tool or a test.
// If a UI framework is ever replaced, nothing in here has to change.
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace proto {

// Every object and asset gets a number that never changes, even if it is renamed
// or moved in the list. Other data refers to things by this ID, never by name or
// by position in a list, so renaming or reordering can't break references.
// 0 means "nothing".
using ObjectId = std::int64_t;
using AssetId = std::int64_t;

// -----------------------------------------------------------------------------
// Images and assets
// -----------------------------------------------------------------------------

// A decoded picture: width * height pixels, each stored as 4 bytes (R, G, B, A).
// This is the plain format both UI frameworks can turn into their own texture
// type (an SDL_Texture for ImGui, a QImage/QPixmap for Qt).
struct ImageRgba {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;  // size = width * height * 4
};

// An imported PNG. `original` is exactly what the file contains; `gbaPreview` is
// the same picture after rounding every color to what the GBA can display
// (5 bits per color channel). The editor shows the preview so the developer sees
// what the hardware will really show, not what their PC shows.
struct Asset {
    AssetId id = 0;
    std::string name;   // e.g. "robot.png"
    std::string path;   // where the original file lives on disk
    ImageRgba original;
    ImageRgba gbaPreview;
    int gbaColorCount = 0;              // distinct opaque colors after conversion
    std::vector<std::string> warnings;  // hardware-limit warnings for the user
};

// -----------------------------------------------------------------------------
// Components
// -----------------------------------------------------------------------------

// A component is a bundle of settings attached to a GameObject (like "draw this
// sprite" or "this is the collision box"). Which settings a component has is
// described by its ComponentDescriptor (see components.hpp); the component
// itself only stores the values, in the same order as the descriptor's list.
//
// Every value is stored as a 64-bit integer. For number properties it is the
// number; for asset-reference properties it is the AssetId (0 = none).
// Keeping one value type makes the prototype small; the real engine will have
// more kinds (fixed-point, enums, object references, ...).
struct Component {
    std::string type;                 // must match a ComponentDescriptor::type
    std::vector<std::int64_t> values; // values[i] belongs to descriptor.properties[i]
};

// A thing in the scene. Position is in GBA screen pixels; (0, 0) is the top-left
// corner of the 240x160 screen. `z` decides draw order in the 2D viewport
// (higher z is drawn on top). In the future isometric profile it becomes height.
struct GameObject {
    ObjectId id = 0;
    std::string name;
    int x = 0;
    int y = 0;
    int z = 0;
    std::vector<Component> components;

    Component* findComponent(const std::string& type);
    const Component* findComponent(const std::string& type) const;
};

// -----------------------------------------------------------------------------
// A tiny isometric height map (feature 13)
// -----------------------------------------------------------------------------

// A grid of tiles, each with a height (0 = flat ground, up to kMaxHeight blocks).
// Stored row by row: heights[tileY * width + tileX].
struct IsoMap {
    static constexpr int kMaxHeight = 4;
    int width = 6;
    int depth = 6;
    std::vector<int> heights = std::vector<int>(36, 0);

    int heightAt(int tileX, int tileY) const { return heights[tileY * width + tileX]; }
    void setHeight(int tileX, int tileY, int h) { heights[tileY * width + tileX] = h; }
};

// -----------------------------------------------------------------------------
// A tiny node graph (feature 14), like a future visual script or state machine
// -----------------------------------------------------------------------------

struct GraphNode {
    int id = 0;
    std::string title;
    int x = 0;  // top-left corner of the node box, in graph coordinates
    int y = 0;
    std::vector<std::string> inputs;   // names of the pins on the left side
    std::vector<std::string> outputs;  // names of the pins on the right side
};

// A wire from one node's output pin to another node's input pin.
struct GraphLink {
    int fromNode = 0;
    int fromPin = 0;  // index into fromNode's outputs
    int toNode = 0;
    int toPin = 0;    // index into toNode's inputs

    bool operator==(const GraphLink&) const = default;
};

struct NodeGraph {
    std::vector<GraphNode> nodes;
    std::vector<GraphLink> links;

    GraphNode* findNode(int nodeId);
    const GraphNode* findNode(int nodeId) const;
};

// -----------------------------------------------------------------------------
// The scene: everything the editor saves to one file
// -----------------------------------------------------------------------------

struct Scene {
    std::string name = "Main";
    std::vector<GameObject> objects;  // list order = order in the Hierarchy panel
    std::vector<Asset> assets;
    IsoMap iso;
    NodeGraph graph;

    // Counter used to hand out new IDs. It is saved with the scene so IDs stay
    // unique after reloading. (The real engine will use random 64-bit IDs so two
    // people can add objects at the same time without clashing.)
    std::int64_t nextId = 1;

    std::int64_t allocateId() { return nextId++; }

    GameObject* findObject(ObjectId id);
    const GameObject* findObject(ObjectId id) const;
    int indexOfObject(ObjectId id) const;  // -1 if not found
    const Asset* findAsset(AssetId id) const;
};

// A new scene with the two example nodes already connected, so the node-graph
// panel has something to show.
Scene makeDefaultScene();

}  // namespace proto
