#pragma once
// =============================================================================
// proto_core/components.hpp — component descriptors ("metadata").
//
// A descriptor is DATA that describes a component type: its name and the list of
// properties it has. The Inspector in BOTH editors is built automatically by
// looping over this list, so adding a new component type means adding one entry
// in components.cpp — no UI code changes at all. That is the "ease of adding new
// component types" test from the framework comparison.
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace proto {

enum class PropertyKind {
    Int,       // a whole number, edited with a number field
    AssetRef,  // a reference to an imported asset (stored as its AssetId)
};

struct PropertyDescriptor {
    std::string name;  // shown in the Inspector, saved in the scene file
    PropertyKind kind = PropertyKind::Int;
    std::int64_t defaultValue = 0;
    int minValue = -32768;
    int maxValue = 32767;
    std::string help;  // tooltip text
};

struct ComponentDescriptor {
    std::string type;         // stable identifier, e.g. "SpriteRenderer"
    std::string displayName;  // label shown to the user
    std::vector<PropertyDescriptor> properties;
};

// All component types the prototypes know about.
const std::vector<ComponentDescriptor>& componentRegistry();

// Looks a descriptor up by type name. Returns nullptr for unknown types.
const ComponentDescriptor* findComponentDescriptor(const std::string& type);

// Property indexes for the built-in SpriteRenderer, used by code that needs to
// read the sprite directly (viewport drawing, code generation).
namespace sprite_renderer {
inline constexpr const char* kType = "SpriteRenderer";
inline constexpr int kSprite = 0;
inline constexpr int kPriority = 1;
}  // namespace sprite_renderer

}  // namespace proto
