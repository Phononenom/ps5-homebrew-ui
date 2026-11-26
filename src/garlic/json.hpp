// Garlic Save Companion - bounded JSON parsing for the Garlic REST API.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace garlic::json
{

enum class NodeTag : std::uint8_t
{
    null,
    boolean,
    number,
    string,
    array,
    object,
};

struct Value
{
    NodeTag type = NodeTag::null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<Value> elements;
    std::vector<std::pair<std::string, std::size_t>> members;

    const Value *member(std::string_view name) const;
};

struct Document
{
    Value root;
    std::string error;
};

Document parse(std::string_view input);
bool string(const Value &object, std::string_view name, std::string *out);
bool integer(const Value &object, std::string_view name, int *out);
bool boolean(const Value &object, std::string_view name, bool *out);
const Value *array(const Value &object, std::string_view name);
bool has_error(const Value &object, std::string *error);

} // namespace garlic::json
