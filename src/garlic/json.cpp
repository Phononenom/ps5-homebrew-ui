// Garlic Save Companion - bounded JSON parsing for the Garlic REST API.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#include "garlic/json.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace garlic::json
{
namespace
{

constexpr std::size_t kMaxDepth = 32;
constexpr std::size_t kMaxValues = 100000;

class Parser
{
  public:
    explicit Parser(std::string_view input) : input_(input)
    {
    }

    Document run()
    {
        Document result;
        if (!value(result.root, 0))
        {
            result.error = error_;
            return result;
        }
        whitespace();
        if (position_ != input_.size())
        {
            result.error = "Trailing data after the JSON value";
            return result;
        }
        return result;
    }

  private:
    bool value(Value &out, std::size_t depth)
    {
        if (depth > kMaxDepth || ++values_ > kMaxValues)
            return fail("JSON nesting or value limit exceeded");
        whitespace();
        if (position_ >= input_.size())
            return fail("Expected a JSON value");
        switch (input_[position_])
        {
        case 'n':
            out.type = NodeTag::null;
            return literal("null");
        case 't':
            out.type = NodeTag::boolean;
            out.boolean = true;
            return literal("true");
        case 'f':
            out.type = NodeTag::boolean;
            out.boolean = false;
            return literal("false");
        case '"':
            out.type = NodeTag::string;
            return string_value(out.text);
        case '[':
            return array_value(out, depth + 1);
        case '{':
            return object_value(out, depth + 1);
        default:
            return number_value(out);
        }
    }

    bool array_value(Value &out, std::size_t depth)
    {
        ++position_;
        out.type = NodeTag::array;
        whitespace();
        if (consume(']'))
            return true;
        for (;;)
        {
            out.elements.emplace_back();
            if (!value(out.elements.back(), depth))
                return false;
            whitespace();
            if (consume(']'))
                return true;
            if (!consume(','))
                return fail("Expected ',' or ']' in array");
        }
    }

    bool object_value(Value &out, std::size_t depth)
    {
        ++position_;
        out.type = NodeTag::object;
        whitespace();
        if (consume('}'))
            return true;
        for (;;)
        {
            whitespace();
            std::string key;
            if (position_ >= input_.size() || input_[position_] != '"')
                return fail("Expected a quoted object key");
            if (!string_value(key))
                return false;
            whitespace();
            if (!consume(':'))
                return fail("Expected ':' after object key");
            Value child;
            if (!value(child, depth))
                return false;
            out.members.emplace_back(std::move(key), out.elements.size());
            out.elements.push_back(std::move(child));
            whitespace();
            if (consume('}'))
                return true;
            if (!consume(','))
                return fail("Expected ',' or '}' in object");
        }
    }

    bool string_value(std::string &out)
    {
        if (!consume('"'))
            return fail("Expected a string");
        out.clear();
        while (position_ < input_.size())
        {
            const unsigned char c = static_cast<unsigned char>(input_[position_++]);
            if (c == '"')
                return true;
            if (c < 0x20)
                return fail("Control character in string");
            if (c != '\\')
            {
                out.push_back(static_cast<char>(c));
                continue;
            }
            if (position_ >= input_.size())
                return fail("Unfinished escape sequence");
            const char escaped = input_[position_++];
            switch (escaped)
            {
            case '"':
                out.push_back('"');
                break;
            case '\\':
                out.push_back('\\');
                break;
            case '/':
                out.push_back('/');
                break;
            case 'b':
                out.push_back('\b');
                break;
            case 'f':
                out.push_back('\f');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'u':
                if (!unicode_escape(out))
                    return false;
                break;
            default:
                return fail("Unknown string escape");
            }
        }
        return fail("Unterminated string");
    }

    bool unicode_escape(std::string &out)
    {
        if (position_ + 4 > input_.size())
            return fail("Short Unicode escape");
        std::uint32_t code = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char c = input_[position_++];
            code <<= 4;
            if (c >= '0' && c <= '9')
                code |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f')
                code |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                code |= static_cast<unsigned>(c - 'A' + 10);
            else
                return fail("Invalid Unicode escape");
        }
        if (code >= 0xD800 && code <= 0xDBFF)
        {
            if (position_ + 6 > input_.size() || input_[position_] != '\\' ||
                input_[position_ + 1] != 'u')
                return fail("High surrogate without low surrogate");
            position_ += 2;
            std::uint32_t low = 0;
            for (int i = 0; i < 4; ++i)
            {
                const char c = input_[position_++];
                low <<= 4;
                if (c >= '0' && c <= '9')
                    low |= static_cast<unsigned>(c - '0');
                else if (c >= 'a' && c <= 'f')
                    low |= static_cast<unsigned>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F')
                    low |= static_cast<unsigned>(c - 'A' + 10);
                else
                    return fail("Invalid low surrogate");
            }
            if (low < 0xDC00 || low > 0xDFFF)
                return fail("Invalid low surrogate");
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
        }
        else if (code >= 0xDC00 && code <= 0xDFFF)
            return fail("Unexpected low surrogate");
        append_utf8(out, code);
        return true;
    }

    static void append_utf8(std::string &out, std::uint32_t code)
    {
        if (code <= 0x7F)
            out.push_back(static_cast<char>(code));
        else if (code <= 0x7FF)
        {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        else if (code <= 0xFFFF)
        {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    bool number_value(Value &out)
    {
        const std::size_t start = position_;
        if (position_ < input_.size() && input_[position_] == '-')
            ++position_;
        if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9')
            return fail("Invalid number");
        if (input_[position_] == '0')
            ++position_;
        else
        {
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9')
                ++position_;
        }
        if (position_ < input_.size() && input_[position_] == '.')
        {
            ++position_;
            if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9')
                return fail("Invalid fraction");
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9')
                ++position_;
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E'))
        {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-'))
                ++position_;
            if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9')
                return fail("Invalid exponent");
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9')
                ++position_;
        }
        const std::string_view token = input_.substr(start, position_ - start);
        out.type = NodeTag::number;
        out.text = token;
        const int converted = std::sscanf(out.text.c_str(), "%lf", &out.number);
        if (converted != 1 || !std::isfinite(out.number))
            return fail("Number is not finite");
        return true;
    }

    bool literal(std::string_view word)
    {
        if (input_.substr(position_, word.size()) != word)
            return fail("Invalid literal");
        position_ += word.size();
        return true;
    }

    void whitespace()
    {
        while (position_ < input_.size())
        {
            const char c = input_[position_];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                break;
            ++position_;
        }
    }

    bool consume(char expected)
    {
        if (position_ >= input_.size() || input_[position_] != expected)
            return false;
        ++position_;
        return true;
    }

    bool fail(std::string_view reason)
    {
        if (error_.empty())
            error_ = std::string(reason) + " at byte " + std::to_string(position_);
        return false;
    }

    std::string_view input_;
    std::size_t position_ = 0;
    std::size_t values_ = 0;
    std::string error_;
};

} // namespace

const Value *Value::member(std::string_view name) const
{
    if (type != NodeTag::object)
        return nullptr;
    for (const auto &entry : members)
        if (entry.first == name && entry.second < elements.size())
            return &elements[entry.second];
    return nullptr;
}

Document parse(std::string_view input)
{
    return Parser(input).run();
}

bool string(const Value &object, std::string_view name, std::string *out)
{
    const Value *value = object.member(name);
    if (value == nullptr || value->type != NodeTag::string || out == nullptr)
        return false;
    *out = value->text;
    return true;
}

bool integer(const Value &object, std::string_view name, int *out)
{
    const Value *value = object.member(name);
    if (value == nullptr || value->type != NodeTag::number || out == nullptr)
        return false;
    const double converted = std::sscanf(value->text.c_str(), "%d", out);
    return converted == 1;
}

bool boolean(const Value &object, std::string_view name, bool *out)
{
    const Value *value = object.member(name);
    if (value == nullptr || value->type != NodeTag::boolean || out == nullptr)
        return false;
    *out = value->boolean;
    return true;
}

const Value *array(const Value &object, std::string_view name)
{
    const Value *value = object.member(name);
    return value != nullptr && value->type == NodeTag::array ? value : nullptr;
}

bool has_error(const Value &object, std::string *error)
{
    std::string message;
    if (!string(object, "error", &message) || message.empty())
        return false;
    if (error != nullptr)
        *error = message;
    return true;
}

} // namespace garlic::json
