#include "proto_core/model.hpp"

#include "proto_core/components.hpp"

#include <utility>

namespace proto {

// The const and non-const versions share one implementation: the non-const one
// calls the const one and removes the const from the result. This is a standard
// C++ trick to avoid writing the same search loop twice.

const Component* GameObject::findComponent(const std::string& type) const {
    for (const Component& component : components) {
        if (component.type == type) return &component;
    }
    return nullptr;
}

Component* GameObject::findComponent(const std::string& type) {
    return const_cast<Component*>(std::as_const(*this).findComponent(type));
}

const GraphNode* NodeGraph::findNode(int nodeId) const {
    for (const GraphNode& node : nodes) {
        if (node.id == nodeId) return &node;
    }
    return nullptr;
}

GraphNode* NodeGraph::findNode(int nodeId) {
    return const_cast<GraphNode*>(std::as_const(*this).findNode(nodeId));
}

const GameObject* Scene::findObject(ObjectId id) const {
    for (const GameObject& object : objects) {
        if (object.id == id) return &object;
    }
    return nullptr;
}

GameObject* Scene::findObject(ObjectId id) {
    return const_cast<GameObject*>(std::as_const(*this).findObject(id));
}

int Scene::indexOfObject(ObjectId id) const {
    for (int i = 0; i < static_cast<int>(objects.size()); ++i) {
        if (objects[i].id == id) return i;
    }
    return -1;
}

const Asset* Scene::findAsset(AssetId id) const {
    for (const Asset& asset : assets) {
        if (asset.id == id) return &asset;
    }
    return nullptr;
}

Scene makeDefaultScene() {
    Scene scene;

    // A couple of raised blocks so the isometric panel shows height straight away.
    scene.iso.setHeight(1, 1, 1);
    scene.iso.setHeight(2, 1, 2);
    scene.iso.setHeight(4, 3, 3);

    // Two example nodes, the way a future visual script might start:
    // "when button A is pressed" -> "play the Jump animation".
    scene.graph.nodes.push_back({1, "On Button Pressed (A)", 20, 40, {}, {"Pressed"}});
    scene.graph.nodes.push_back({2, "Play Animation (Jump)", 260, 80, {"Run"}, {"Done"}});
    scene.graph.links.push_back({1, 0, 2, 0});
    return scene;
}

}  // namespace proto
