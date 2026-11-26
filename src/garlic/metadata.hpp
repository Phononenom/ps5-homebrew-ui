// Garlic Save Companion - read-only game artwork from installed title metadata.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "garlic/image.hpp"

#include <string>
#include <string_view>

namespace garlic
{

bool parse_pasv_port(std::string_view response, int *port);
std::string title_icon_path(std::string_view title_id);
std::string title_photo_path(std::string_view title_id);
bool fetch_title_icon(std::string_view title_id, Image *out, std::string *error);

} // namespace garlic
