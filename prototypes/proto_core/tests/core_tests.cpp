// Automated tests for the shared prototype model. These run without any window,
// which is the point: everything the two editors share can be checked headlessly.
//
// Run: build the `proto_core_tests` target, then execute it (see prototypes/README.md).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "proto_core/assets.hpp"
#include "proto_core/codegen.hpp"
#include "proto_core/commands.hpp"
#include "proto_core/components.hpp"
#include "proto_core/geometry.hpp"
#include "proto_core/serialization.hpp"

#include <filesystem>

using namespace proto;

// PROTO_SAMPLE_ASSETS is defined by CMake as the absolute path of sample_assets/.
static std::string sampleAsset(const char* file) {
    return (std::filesystem::path(PROTO_SAMPLE_ASSETS) / file).string();
}

TEST_CASE("create, undo and redo keep the same object ID") {
    Scene scene;
    CommandHistory history;
    auto create = std::make_unique<CreateObjectCommand>("Player", 10, 20);
    CreateObjectCommand* createPtr = create.get();
    history.execute(std::move(create), scene);
    REQUIRE(scene.objects.size() == 1);
    ObjectId id = createPtr->createdId();

    history.undo(scene);
    CHECK(scene.objects.empty());
    history.redo(scene);
    REQUIRE(scene.objects.size() == 1);
    CHECK(scene.objects[0].id == id);
    CHECK(scene.objects[0].x == 10);
}

TEST_CASE("a drag merges into one undo step, a new drag does not") {
    Scene scene;
    CommandHistory history;
    history.execute(std::make_unique<CreateObjectCommand>("Box", 0, 0), scene);
    ObjectId id = scene.objects[0].id;

    for (int i = 1; i <= 30; ++i)  // 30 mouse-move events in drag session 7
        history.execute(std::make_unique<SetPositionCommand>(id, i, i * 2, 0, 7), scene);
    CHECK(history.undoCount() == 2);  // create + one merged move
    CHECK(scene.objects[0].x == 30);

    history.execute(std::make_unique<SetPositionCommand>(id, 99, 99, 0, 8), scene);
    CHECK(history.undoCount() == 3);  // different session: separate step

    history.undo(scene);
    CHECK(scene.objects[0].x == 30);
    history.undo(scene);
    CHECK(scene.objects[0].x == 0);  // back to where the first drag started
}

TEST_CASE("delete restores object at its original list position") {
    Scene scene;
    CommandHistory history;
    for (const char* name : {"A", "B", "C"})
        history.execute(std::make_unique<CreateObjectCommand>(name, 0, 0), scene);
    ObjectId middle = scene.objects[1].id;
    history.execute(std::make_unique<DeleteObjectCommand>(middle), scene);
    CHECK(scene.objects.size() == 2);
    history.undo(scene);
    REQUIRE(scene.objects.size() == 3);
    CHECK(scene.objects[1].name == "B");
}

TEST_CASE("components: add, edit, remove, all undoable") {
    Scene scene;
    CommandHistory history;
    history.execute(std::make_unique<CreateObjectCommand>("Enemy", 0, 0), scene);
    ObjectId id = scene.objects[0].id;

    history.execute(std::make_unique<AddComponentCommand>(id, "Collider"), scene);
    REQUIRE(scene.objects[0].findComponent("Collider"));
    CHECK(scene.objects[0].findComponent("Collider")->values[0] == 16);  // descriptor default

    history.execute(std::make_unique<SetPropertyCommand>(id, "Collider", 0, 24, 1), scene);
    CHECK(scene.objects[0].findComponent("Collider")->values[0] == 24);

    history.execute(std::make_unique<RemoveComponentCommand>(id, "Collider"), scene);
    CHECK_FALSE(scene.objects[0].findComponent("Collider"));
    history.undo(scene);
    REQUIRE(scene.objects[0].findComponent("Collider"));
    CHECK(scene.objects[0].findComponent("Collider")->values[0] == 24);
}

TEST_CASE("assigning a sprite adds a SpriteRenderer, and undo removes it again") {
    Scene scene;
    CommandHistory history;
    history.execute(std::make_unique<CreateObjectCommand>("Hero", 0, 0), scene);
    ObjectId id = scene.objects[0].id;
    history.execute(std::make_unique<AssignSpriteCommand>(id, 42), scene);
    REQUIRE(scene.objects[0].findComponent(sprite_renderer::kType));
    CHECK(scene.objects[0].findComponent(sprite_renderer::kType)->values[sprite_renderer::kSprite] == 42);
    history.undo(scene);
    CHECK_FALSE(scene.objects[0].findComponent(sprite_renderer::kType));
}

TEST_CASE("PNG import converts colors to GBA 15-bit and counts them") {
    std::string error;
    auto robot = importPng(sampleAsset("robot.png"), error);
    REQUIRE_MESSAGE(robot, error);
    CHECK(robot->original.width == 16);
    CHECK(robot->original.height == 16);
    CHECK(robot->gbaColorCount == 5);  // k, g, w, c, r in make_sample_sprites.py
    CHECK(robot->warnings.empty());
    CHECK(toGbaChannel(255) == 255);
    CHECK(toGbaChannel(0) == 0);
    CHECK(toGbaChannel(7) == 0);   // lowest 3 bits are lost on the GBA
    CHECK(toGbaChannel(8) == 8);

    CHECK_FALSE(importPng(sampleAsset("does_not_exist.png"), error));
    CHECK_FALSE(error.empty());
}

static Scene makeTestScene() {
    Scene scene = makeDefaultScene();
    std::string error;
    auto robot = importPng(sampleAsset("robot.png"), error);
    REQUIRE(robot);
    robot->id = scene.allocateId();
    scene.assets.push_back(*robot);
    CommandHistory history;
    history.execute(std::make_unique<CreateObjectCommand>("Player", 100, 60, robot->id), scene);
    history.execute(std::make_unique<CreateObjectCommand>("Coin", 30, 40), scene);
    history.execute(std::make_unique<CreateObjectCommand>("Coin", 50, 40), scene);
    history.execute(std::make_unique<AddComponentCommand>(scene.objects[1].id, "Collider"), scene);
    return scene;
}

TEST_CASE("save -> load -> save produces byte-identical JSON") {
    Scene scene = makeTestScene();
    std::string baseDir = std::filesystem::path(PROTO_SAMPLE_ASSETS).parent_path().string();
    std::string first = sceneToJson(scene, baseDir);
    std::string error;
    auto loaded = sceneFromJson(first, error, baseDir);
    REQUIRE_MESSAGE(loaded, error);
    CHECK(sceneToJson(*loaded, baseDir) == first);
    CHECK(loaded->assets[0].gbaColorCount == 5);  // asset was re-imported from disk
    CHECK(first.find("\"sample_assets/robot.png\"") != std::string::npos);  // relative path
}

TEST_CASE("loading garbage gives an error instead of crashing") {
    std::string error;
    CHECK_FALSE(sceneFromJson("{ not json", error));
    CHECK_FALSE(error.empty());
    CHECK_FALSE(sceneFromJson(R"({"format":"something.else"})", error));
}

TEST_CASE("generated source is deterministic and uses unique identifiers") {
    Scene scene = makeTestScene();
    std::string a = generateSceneSource(scene);
    std::string b = generateSceneSource(makeTestScene());
    CHECK(a == b);
    CHECK(a.find("gen_scene_main.cpp") != std::string::npos);
    CHECK(a.find("gen_obj_coin ") != std::string::npos);
    CHECK(a.find("gen_obj_coin_2 ") != std::string::npos);
    CHECK(a.find("Starts at pixel (100, 60)") != std::string::npos);
    CHECK(toIdentifier("Player 1!") == "player_1");
    CHECK(toIdentifier("2nd Boss") == "n_2nd_boss");
}

TEST_CASE("viewport picking: the object drawn on top wins") {
    Scene scene;
    CommandHistory history;
    history.execute(std::make_unique<CreateObjectCommand>("Back", 10, 10), scene);   // 16x16 box
    history.execute(std::make_unique<CreateObjectCommand>("Front", 20, 20), scene);  // overlaps Back
    const ObjectId back = scene.objects[0].id;
    const ObjectId front = scene.objects[1].id;

    CHECK(pickObject(scene, 12, 12) == back);
    CHECK(pickObject(scene, 22, 22) == front);  // same z: later in the list is on top
    CHECK(pickObject(scene, 100, 100) == 0);
    CHECK(pickObject(scene, 25, 10) == back);  // a 16-pixel box at x=10 covers x=10..25
    CHECK(pickObject(scene, 26, 10) == 0);
    CHECK(pickObject(scene, 25, 9) == 0);

    history.execute(std::make_unique<SetPositionCommand>(back, 10, 10, 5, 1), scene);  // raise Back
    CHECK(pickObject(scene, 22, 22) == back);
}

TEST_CASE("isometric projection and picking agree") {
    IsoProjection proj;
    CHECK(isoToScreen(0, 0, 0, proj).x == 0);
    CHECK(isoToScreen(1, 0, 0, proj).x == 16);
    CHECK(isoToScreen(1, 0, 0, proj).y == 8);
    CHECK(isoToScreen(0, 1, 0, proj).x == -16);
    CHECK(isoToScreen(0, 0, 2, proj).y == -16);

    auto centerOf = [&](const IsoMap& m, int x, int y) {
        Point top = isoToScreen(x, y, m.heightAt(x, y), proj);
        return Point{top.x, top.y + proj.tileHeight / 2};
    };

    // 1) On a flat map, the center of every tile picks that tile.
    IsoMap flat;
    for (int y = 0; y < flat.depth; ++y) {
        for (int x = 0; x < flat.width; ++x) {
            auto picked = isoPickTile(flat, centerOf(flat, x, y), proj);
            REQUIRE(picked);
            CHECK(picked->x == x);
            CHECK(picked->y == y);
        }
    }

    // 2) A raised tile is picked at its raised top face.
    IsoMap map;
    map.setHeight(2, 2, 3);
    auto raised = isoPickTile(map, centerOf(map, 2, 2), proj);
    REQUIRE(raised);
    CHECK(raised->x == 2);
    CHECK(raised->y == 2);

    // 3) Tile (1,1) sits directly behind the tall column (2,2), whose top face now
    //    covers (1,1)'s center on screen, so clicking there must hit the column:
    //    you can only pick what you can see.
    auto hidden = isoPickTile(map, centerOf(map, 1, 1), proj);
    REQUIRE(hidden);
    CHECK(hidden->x == 2);
    CHECK(hidden->y == 2);
}

TEST_CASE("node graph: connecting replaces the old wire into an input") {
    Scene scene = makeDefaultScene();
    NodeGraph& g = scene.graph;
    REQUIRE(g.links.size() == 1);
    CHECK_FALSE(connectPins(g, 2, 0, 2, 0));  // self-connection refused
    CHECK(connectPins(g, 1, 0, 2, 0));
    CHECK(g.links.size() == 1);
    CHECK(disconnectInput(g, 2, 0));
    CHECK(g.links.empty());

    int node = 0, pin = 0;
    bool output = false;
    Point p = pinPosition(*g.findNode(1), true, 0);
    REQUIRE(hitTestPin(g, p, node, output, pin));
    CHECK(node == 1);
    CHECK(output);
}

TEST_CASE("every registered component has defaults inside its min/max range") {
    for (const ComponentDescriptor& d : componentRegistry()) {
        Component c = makeDefaultComponent(d.type);
        CHECK(c.type == d.type);
        REQUIRE(c.values.size() == d.properties.size());
        for (size_t i = 0; i < d.properties.size(); ++i) {
            if (d.properties[i].kind != PropertyKind::Int) continue;
            CHECK(c.values[i] >= d.properties[i].minValue);
            CHECK(c.values[i] <= d.properties[i].maxValue);
        }
    }
}
