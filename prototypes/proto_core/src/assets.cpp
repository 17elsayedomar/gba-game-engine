#include "proto_core/assets.hpp"

#include <filesystem>
#include <set>

// stb_image is a single-file PNG/JPG decoder. Defining STB_IMAGE_IMPLEMENTATION in
// exactly ONE .cpp file tells the header to include its function bodies here.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505)  // stb has helper functions we don't use; not our code
#endif
#include "stb/stb_image.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace proto {

std::uint8_t toGbaChannel(std::uint8_t value) {
    // 8 bits -> 5 bits: throw away the 3 lowest bits (value >> 3, range 0..31).
    // 5 bits -> 8 bits for display: shift back up and copy the top bits into the
    // empty low bits, so 31 becomes 255 (pure white stays pure white).
    std::uint8_t five = value >> 3;
    return static_cast<std::uint8_t>((five << 3) | (five >> 2));
}

// The GBA hardware can only draw sprites in these sizes (width x height).
static bool isGbaSpriteSize(int w, int h) {
    static const int kSizes[][2] = {{8, 8},   {16, 16}, {32, 32}, {64, 64}, {16, 8},
                                    {32, 8},  {32, 16}, {64, 32}, {8, 16},  {8, 32},
                                    {16, 32}, {32, 64}};
    for (const auto& size : kSizes) {
        if (size[0] == w && size[1] == h) return true;
    }
    return false;
}

std::optional<Asset> importPng(const std::string& path, std::string& error) {
    int width = 0, height = 0, channelsInFile = 0;
    // Ask for 4 channels (RGBA) no matter what the file contains.
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channelsInFile, 4);
    if (!data) {
        error = "Could not load '" + path + "': " + stbi_failure_reason();
        return std::nullopt;
    }

    Asset asset;
    asset.path = path;
    asset.name = std::filesystem::path(path).filename().string();
    asset.original.width = width;
    asset.original.height = height;
    asset.original.pixels.assign(data, data + static_cast<size_t>(width) * height * 4);
    stbi_image_free(data);

    // Build the GBA preview and count colors at the same time.
    // Pixels with alpha below 128 count as fully transparent (the GBA has no
    // partial transparency for sprite pixels: color index 0 is "see-through").
    asset.gbaPreview = asset.original;
    std::set<std::uint32_t> colors;
    std::vector<std::uint8_t>& px = asset.gbaPreview.pixels;
    for (size_t i = 0; i < px.size(); i += 4) {
        if (px[i + 3] < 128) {
            px[i] = px[i + 1] = px[i + 2] = px[i + 3] = 0;
            continue;
        }
        px[i] = toGbaChannel(px[i]);
        px[i + 1] = toGbaChannel(px[i + 1]);
        px[i + 2] = toGbaChannel(px[i + 2]);
        px[i + 3] = 255;
        colors.insert((std::uint32_t(px[i]) << 16) | (std::uint32_t(px[i + 1]) << 8) | px[i + 2]);
    }
    asset.gbaColorCount = static_cast<int>(colors.size());

    // Warnings: explain the limit, not just that something is wrong.
    if (!isGbaSpriteSize(width, height)) {
        asset.warnings.push_back("Size " + std::to_string(width) + "x" + std::to_string(height) +
                                 " is not a GBA sprite size (8/16/32/64 combinations).");
    }
    if (asset.gbaColorCount > 255) {
        asset.warnings.push_back("More than 255 colors: cannot fit any GBA palette.");
    } else if (asset.gbaColorCount > 15) {
        asset.warnings.push_back(std::to_string(asset.gbaColorCount) +
                                 " colors: needs 8bpp (256-color) mode. 4bpp sprites "
                                 "allow 15 colors + transparent.");
    }
    return asset;
}

}  // namespace proto
