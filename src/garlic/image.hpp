// Garlic Save Companion - bounded decoding of save and title artwork.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace garlic
{

struct Image
{
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

bool decode_png(std::string_view data, Image *out, std::string *error);

} // namespace garlic
