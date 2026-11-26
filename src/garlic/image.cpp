// Garlic Save Companion - bounded decoding of save and title artwork.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#include "garlic/image.hpp"

#include "../../third_party/stb/stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace garlic
{
namespace
{

constexpr std::size_t kMaxEncodedBytes = 16u << 20;
constexpr int kMaxInputDimension = 4096;
constexpr std::size_t kMaxInputPixels = 16u << 20;
constexpr int kMaxTextureDimension = 1024;

bool supported_dimensions(int width, int height)
{
    return width > 0 && height > 0 && width <= kMaxInputDimension && height <= kMaxInputDimension &&
           static_cast<std::size_t>(width) * static_cast<std::size_t>(height) <= kMaxInputPixels;
}

void resample(const unsigned char *pixels, int width, int height, Image *out)
{
    const float scale = std::min(1.0f, static_cast<float>(kMaxTextureDimension) /
                                           static_cast<float>(std::max(width, height)));
    out->width = std::max(1, static_cast<int>(static_cast<float>(width) * scale));
    out->height = std::max(1, static_cast<int>(static_cast<float>(height) * scale));
    out->rgba.resize(static_cast<std::size_t>(out->width) * static_cast<std::size_t>(out->height) *
                     4);
    if (out->width == width && out->height == height)
    {
        std::copy(pixels, pixels + out->rgba.size(), out->rgba.data());
        return;
    }
    for (int y = 0; y < out->height; ++y)
    {
        const float source_y = (static_cast<float>(y) + 0.5f) * height / out->height - 0.5f;
        const int y0 = std::clamp(static_cast<int>(std::floor(source_y)), 0, height - 1);
        const int y1 = std::min(y0 + 1, height - 1);
        const float fy = std::clamp(source_y - static_cast<float>(y0), 0.0f, 1.0f);
        for (int x = 0; x < out->width; ++x)
        {
            const float source_x = (static_cast<float>(x) + 0.5f) * width / out->width - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(source_x)), 0, width - 1);
            const int x1 = std::min(x0 + 1, width - 1);
            const float fx = std::clamp(source_x - static_cast<float>(x0), 0.0f, 1.0f);
            for (std::size_t channel = 0; channel < 4; ++channel)
            {
                const auto sample = [&](int sx, int sy) {
                    return static_cast<float>(
                        pixels[(static_cast<std::size_t>(sy) * width + sx) * 4 + channel]);
                };
                const float top = sample(x0, y0) + (sample(x1, y0) - sample(x0, y0)) * fx;
                const float bottom = sample(x0, y1) + (sample(x1, y1) - sample(x0, y1)) * fx;
                const float value = top + (bottom - top) * fy;
                out->rgba[(static_cast<std::size_t>(y) * out->width + x) * 4 + channel] =
                    static_cast<unsigned char>(std::clamp(value + 0.5f, 0.0f, 255.0f));
            }
        }
    }
}

std::uint32_t big_endian(const unsigned char *bytes)
{
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
}

} // namespace

bool decode_png(std::string_view data, Image *out, std::string *error)
{
    auto fail = [&](const std::string &message)
    {
        if (error != nullptr)
            *error = message;
        return false;
    };
    if (out == nullptr)
        return fail("No destination for the game photo");
    *out = {};
    if (data.size() < 24 || data.size() > kMaxEncodedBytes)
        return fail("The game photo has an invalid size");
    constexpr unsigned char signature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    const auto *bytes = reinterpret_cast<const unsigned char *>(data.data());
    if (!std::equal(std::begin(signature), std::end(signature), bytes))
        return fail("The mounted icon is not a PNG image");
    const std::uint32_t width = big_endian(bytes + 16);
    const std::uint32_t height = big_endian(bytes + 20);
    if (width == 0 || height == 0 || width > static_cast<std::uint32_t>(kMaxInputDimension) ||
        height > static_cast<std::uint32_t>(kMaxInputDimension) ||
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) > kMaxInputPixels)
        return fail("The game photo dimensions are not supported");

    int actual_width = 0;
    int actual_height = 0;
    int channels = 0;
    unsigned char *decoded = stbi_load_from_memory(bytes, static_cast<int>(data.size()),
                                                   &actual_width, &actual_height, &channels, 4);
    if (decoded == nullptr)
    {
        const char *reason = stbi_failure_reason();
        return fail(std::string("Could not decode the game photo") +
                    (reason != nullptr ? std::string(": ") + reason : std::string()));
    }
    if (!supported_dimensions(actual_width, actual_height))
    {
        stbi_image_free(decoded);
        return fail("The decoded game photo dimensions are not supported");
    }
    resample(decoded, actual_width, actual_height, out);
    stbi_image_free(decoded);
    return true;
}

} // namespace garlic
