#pragma once
// =============================================================================
// proto_core/serialization.hpp — saving and loading scenes as JSON (feature 11).
//
// "Serialization" means turning data in memory into text (or bytes) that can be
// written to a file, and "deserialization" is the reverse.
//
// The output is DETERMINISTIC: the same scene always produces exactly the same
// text (keys sorted alphabetically, 2-space indent, "\n" line endings). That makes
// Git diffs readable and lets tests compare files byte by byte.
// =============================================================================

#include "proto_core/model.hpp"

#include <optional>
#include <string>

namespace proto {

// `baseDir` is the folder the scene file lives in. Asset paths are written
// relative to it (with "/" separators), so the file is the same on every
// computer and the project folder can be moved.
std::string sceneToJson(const Scene& scene, const std::string& baseDir = "");

// Parses JSON produced by sceneToJson. Asset pixels are not stored in the file;
// assets are re-imported from their path (relative to `baseDir`) when loading.
// Assets whose file is missing are kept (so references stay valid) but get a
// warning and no pixels.
std::optional<Scene> sceneFromJson(const std::string& text, std::string& error,
                                   const std::string& baseDir = "");

bool saveSceneFile(const Scene& scene, const std::string& path, std::string& error);
std::optional<Scene> loadSceneFile(const std::string& path, std::string& error);

}  // namespace proto
