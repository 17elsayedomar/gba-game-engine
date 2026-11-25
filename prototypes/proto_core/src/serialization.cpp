#include "proto_core/serialization.hpp"

#include "proto_core/assets.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

// nlohmann::json stores objects in a std::map, which keeps keys sorted
// alphabetically. That is what makes the output deterministic.
#include "nlohmann/json.hpp"

namespace proto {

using nlohmann::json;

static constexpr const char* kFormat = "gbe.prototype.scene";
static constexpr int kFormatVersion = 1;

namespace fs = std::filesystem;

// Turns an absolute asset path into one relative to the scene's folder.
static std::string relativeAssetPath(const std::string& path, const std::string& baseDir) {
    if (baseDir.empty()) return fs::path(path).generic_string();
    std::error_code ec;
    fs::path relative = fs::relative(fs::path(path), fs::path(baseDir), ec);
    if (ec || relative.empty()) return fs::path(path).generic_string();
    return relative.generic_string();
}

static std::string resolveAssetPath(const std::string& stored, const std::string& baseDir) {
    fs::path p(stored);
    if (p.is_absolute() || baseDir.empty()) return p.string();
    return (fs::path(baseDir) / p).lexically_normal().string();
}

std::string sceneToJson(const Scene& scene, const std::string& baseDir) {
    json root;
    root["format"] = kFormat;
    root["formatVersion"] = kFormatVersion;
    root["name"] = scene.name;
    root["nextId"] = scene.nextId;

    json assets = json::array();
    for (const Asset& asset : scene.assets) {
        assets.push_back({{"id", asset.id},
                          {"name", asset.name},
                          {"path", relativeAssetPath(asset.path, baseDir)}});
    }
    root["assets"] = assets;

    json objects = json::array();
    for (const GameObject& object : scene.objects) {
        json components = json::array();
        for (const Component& component : object.components) {
            components.push_back({{"type", component.type}, {"values", component.values}});
        }
        objects.push_back({{"id", object.id},
                           {"name", object.name},
                           {"position", {object.x, object.y, object.z}},
                           {"components", components}});
    }
    root["objects"] = objects;

    root["iso"] = {{"width", scene.iso.width},
                   {"depth", scene.iso.depth},
                   {"heights", scene.iso.heights}};

    json nodes = json::array();
    for (const GraphNode& node : scene.graph.nodes) {
        nodes.push_back({{"id", node.id},
                         {"title", node.title},
                         {"position", {node.x, node.y}},
                         {"inputs", node.inputs},
                         {"outputs", node.outputs}});
    }
    json links = json::array();
    for (const GraphLink& link : scene.graph.links) {
        links.push_back({link.fromNode, link.fromPin, link.toNode, link.toPin});
    }
    root["graph"] = {{"nodes", nodes}, {"links", links}};

    return root.dump(2) + "\n";
}

std::optional<Scene> sceneFromJson(const std::string& text, std::string& error,
                                   const std::string& baseDir) {
    // nlohmann::json reports problems by throwing exceptions. Exceptions are fine
    // in desktop tools (the GBA runtime will not use them); we catch them here and
    // turn them into a readable error message.
    try {
        json root = json::parse(text);
        if (root.value("format", "") != kFormat) {
            error = "Not a prototype scene file (missing or wrong \"format\").";
            return std::nullopt;
        }
        if (root.value("formatVersion", 0) != kFormatVersion) {
            error = "Unsupported formatVersion.";
            return std::nullopt;
        }

        Scene scene;
        scene.name = root.at("name").get<std::string>();
        scene.nextId = root.at("nextId").get<std::int64_t>();

        for (const json& a : root.at("assets")) {
            std::string importError;
            Asset asset;
            std::string fullPath = resolveAssetPath(a.at("path").get<std::string>(), baseDir);
            if (auto imported = importPng(fullPath, importError)) {
                asset = std::move(*imported);
            } else {
                asset.path = fullPath;
                asset.warnings.push_back("Missing file: " + importError);
            }
            asset.id = a.at("id").get<AssetId>();
            asset.name = a.at("name").get<std::string>();
            scene.assets.push_back(std::move(asset));
        }

        for (const json& o : root.at("objects")) {
            GameObject object;
            object.id = o.at("id").get<ObjectId>();
            object.name = o.at("name").get<std::string>();
            const json& position = o.at("position");
            object.x = position.at(0).get<int>();
            object.y = position.at(1).get<int>();
            object.z = position.at(2).get<int>();
            for (const json& c : o.at("components")) {
                object.components.push_back(
                    {c.at("type").get<std::string>(), c.at("values").get<std::vector<std::int64_t>>()});
            }
            scene.objects.push_back(std::move(object));
        }

        const json& iso = root.at("iso");
        scene.iso.width = iso.at("width").get<int>();
        scene.iso.depth = iso.at("depth").get<int>();
        scene.iso.heights = iso.at("heights").get<std::vector<int>>();
        if (static_cast<int>(scene.iso.heights.size()) != scene.iso.width * scene.iso.depth) {
            error = "iso.heights has the wrong number of entries.";
            return std::nullopt;
        }

        const json& graph = root.at("graph");
        for (const json& n : graph.at("nodes")) {
            GraphNode node;
            node.id = n.at("id").get<int>();
            node.title = n.at("title").get<std::string>();
            node.x = n.at("position").at(0).get<int>();
            node.y = n.at("position").at(1).get<int>();
            node.inputs = n.at("inputs").get<std::vector<std::string>>();
            node.outputs = n.at("outputs").get<std::vector<std::string>>();
            scene.graph.nodes.push_back(std::move(node));
        }
        for (const json& l : graph.at("links")) {
            scene.graph.links.push_back(
                {l.at(0).get<int>(), l.at(1).get<int>(), l.at(2).get<int>(), l.at(3).get<int>()});
        }
        return scene;
    } catch (const std::exception& e) {
        error = std::string("Invalid scene file: ") + e.what();
        return std::nullopt;
    }
}

bool saveSceneFile(const Scene& scene, const std::string& path, std::string& error) {
    // Binary mode: write "\n" exactly as-is, so Windows doesn't turn it into "\r\n".
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error = "Could not open '" + path + "' for writing.";
        return false;
    }
    file << sceneToJson(scene, fs::absolute(path).parent_path().string());
    return static_cast<bool>(file);
}

std::optional<Scene> loadSceneFile(const std::string& path, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Could not open '" + path + "'.";
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return sceneFromJson(buffer.str(), error, fs::absolute(path).parent_path().string());
}

}  // namespace proto
