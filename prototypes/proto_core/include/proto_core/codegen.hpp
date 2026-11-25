#pragma once
// =============================================================================
// proto_core/codegen.hpp — the "generated source" preview (feature 12).
//
// A miniature version of the engine's deterministic code generator: it turns the
// scene into readable C++ with explanatory comments built from templates and the
// scene's own names. No AI, no randomness, no timestamps: the same scene always
// produces exactly the same text, which the tests check.
//
// The output is ILLUSTRATIVE ONLY — the runtime API it calls (gbe::...) does not
// exist yet. The real generator arrives in milestone M1.2.
// =============================================================================

#include "proto_core/model.hpp"

#include <string>

namespace proto {

// Turns a display name into a valid C++ identifier: "Player 1!" -> "player_1".
std::string toIdentifier(const std::string& name);

// Returns the contents of the would-be file "gen_scene_<name>.cpp".
std::string generateSceneSource(const Scene& scene);

}  // namespace proto
