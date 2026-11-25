#pragma once
// =============================================================================
// proto_core/assets.hpp — importing PNG files (feature 4).
//
// Decoding the PNG happens HERE, in shared code, not in the UI. Both editors get
// the same pixels, the same GBA color preview and the same warnings. The UIs only
// turn the finished pixels into their own texture type for drawing.
// =============================================================================

#include "proto_core/model.hpp"

#include <optional>
#include <string>

namespace proto {

// Loads a PNG and fills in everything except `id`. Returns std::nullopt and sets
// `error` if the file can't be read or isn't a valid image.
std::optional<Asset> importPng(const std::string& path, std::string& error);

// Rounds one 8-bit color channel to the GBA's 5 bits and back to 8 bits, which is
// exactly how the color will look on the console. (GBA colors are 15-bit:
// 5 bits each for red, green and blue — 32 shades per channel instead of 256.)
std::uint8_t toGbaChannel(std::uint8_t value);

}  // namespace proto
