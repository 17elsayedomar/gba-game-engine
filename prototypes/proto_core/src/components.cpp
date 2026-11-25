#include "proto_core/components.hpp"

namespace proto {

const std::vector<ComponentDescriptor>& componentRegistry() {
    // Built once, the first time anyone asks ("function-local static").
    // To add a new component type to BOTH editors, add an entry here.
    static const std::vector<ComponentDescriptor> registry = {
        {
            "SpriteRenderer",
            "Sprite Renderer",
            {
                {"sprite", PropertyKind::AssetRef, 0, 0, 0,
                 "Which imported image to draw. Drag an asset here."},
                {"priority", PropertyKind::Int, 1, 0, 3,
                 "GBA OBJ priority: 0 draws in front of every background, 3 behind most."},
            },
        },
        {
            "Collider",
            "Box Collider",
            {
                {"width", PropertyKind::Int, 16, 1, 240, "Collision box width in pixels."},
                {"height", PropertyKind::Int, 16, 1, 160, "Collision box height in pixels."},
            },
        },
        {
            "Health",
            "Health",
            {
                {"maxHealth", PropertyKind::Int, 3, 1, 99, "Hit points when the object spawns."},
            },
        },
    };
    return registry;
}

const ComponentDescriptor* findComponentDescriptor(const std::string& type) {
    for (const ComponentDescriptor& descriptor : componentRegistry()) {
        if (descriptor.type == type) return &descriptor;
    }
    return nullptr;
}

}  // namespace proto
