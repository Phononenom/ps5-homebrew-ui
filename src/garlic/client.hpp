// Garlic Save Companion - bounded loopback HTTP and payload transport.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace garlic
{

struct HttpResponse
{
    bool ok = false;
    int status = 0;
    std::string body;
    std::string error;
};

HttpResponse parse_http(std::string_view input);
HttpResponse get(std::string_view path, int timeout_seconds = 3);
bool send_payload(const std::vector<std::uint8_t> &data, std::string *error);

} // namespace garlic
