// Garlic Save Companion - Garlic REST models and protocol helpers.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#include "garlic/protocol.hpp"

#include <algorithm>
#include <cstdio>

namespace garlic
{
namespace
{

bool parsed(const json::Document &document, const json::Value **root, std::string *error)
{
    if (!document.error.empty())
    {
        *error = document.error;
        return false;
    }
    if (document.root.type != json::NodeTag::object)
    {
        *error = "Expected a JSON object";
        return false;
    }
    if (json::has_error(document.root, error))
        return false;
    *root = &document.root;
    return true;
}

bool file(const json::Value &value, FileEntry *out, std::string *error)
{
    if (value.type != json::NodeTag::object)
    {
        *error = "File entry is not an object";
        return false;
    }
    if (!json::string(value, "name", &out->name) || !json::boolean(value, "dir", &out->directory))
    {
        *error = "File entry is missing name or dir";
        return false;
    }
    const json::Value *size = value.member("size");
    if (size == nullptr || size->type != json::NodeTag::number)
    {
        *error = "File entry is missing size";
        return false;
    }
    out->size = static_cast<std::int64_t>(size->number);
    return true;
}

bool files(const json::Value &object, std::string_view key, std::vector<FileEntry> *out,
           std::string *error)
{
    const json::Value *items = json::array(object, key);
    if (items == nullptr)
    {
        *error = "Missing files array";
        return false;
    }
    out->clear();
    out->reserve(items->elements.size());
    for (const json::Value &item : items->elements)
    {
        out->emplace_back();
        if (!file(item, &out->back(), error))
            return false;
    }
    return true;
}

} // namespace

bool parse_status(std::string_view input, Status *out, std::string *error)
{
    const json::Value *root = nullptr;
    const json::Document document = json::parse(input);
    if (out == nullptr || !parsed(document, &root, error))
        return false;
    if (!json::boolean(*root, "mounted", &out->mounted))
    {
        *error = "Missing mounted state";
        return false;
    }
    (void)json::string(*root, "mount_point", &out->mount_point);
    (void)json::string(*root, "save_path", &out->save_path);
    constexpr std::string_view home = "/user/home/";
    if (out->save_path.starts_with(home))
    {
        const std::size_t end = out->save_path.find('/', home.size());
        out->uid = out->save_path.substr(home.size(), end - home.size());
    }
    return true;
}

bool parse_saves(std::string_view input, std::vector<Save> *out, std::string *error)
{
    const json::Value *root = nullptr;
    const json::Document document = json::parse(input);
    if (out == nullptr || !parsed(document, &root, error))
        return false;
    const json::Value *items = json::array(*root, "saves");
    if (items == nullptr)
    {
        *error = "Missing saves array";
        return false;
    }
    out->clear();
    out->reserve(items->elements.size());
    for (const json::Value &item : items->elements)
    {
        if (item.type != json::NodeTag::object)
        {
            *error = "Save entry is not an object";
            return false;
        }
        Save save;
        save.index = static_cast<int>(out->size());
        if (!json::string(item, "title_id", &save.title_id) ||
            !json::string(item, "save_name", &save.save_name))
        {
            *error = "Save entry is missing title_id or save_name";
            return false;
        }
        (void)json::string(item, "path", &save.path);
        (void)json::string(item, "dir", &save.dir);
        (void)json::string(item, "title_name", &save.title_name);
        (void)json::string(item, "type", &save.type);
        (void)json::boolean(item, "backup", &save.backup);
        (void)json::boolean(item, "usb", &save.usb);
        (void)json::string(item, "uid", &save.uid);
        out->push_back(std::move(save));
    }
    return true;
}

bool parse_mount(std::string_view input, Mount *out, std::string *error)
{
    const json::Value *root = nullptr;
    const json::Document document = json::parse(input);
    if (out == nullptr || !parsed(document, &root, error))
        return false;
    if (json::has_error(*root, error))
        return false;
    (void)json::string(*root, "title_id", &out->title_id);
    (void)json::string(*root, "save_name", &out->save_name);
    (void)json::string(*root, "mount", &out->mount);
    (void)json::string(*root, "account_id", &out->account_id);
    (void)json::string(*root, "save_title", &out->save_title);
    (void)json::boolean(*root, "sfo_zeroed", &out->sfo_zeroed);
    (void)json::boolean(*root, "db_found", &out->db_found);
    return files(*root, "files", &out->files, error);
}

bool parse_files(std::string_view input, std::vector<FileEntry> *out, std::string *error)
{
    const json::Value *root = nullptr;
    const json::Document document = json::parse(input);
    return out != nullptr && parsed(document, &root, error) && files(*root, "files", out, error);
}

bool parse_operation(std::string_view input, std::string *error)
{
    const json::Value *root = nullptr;
    const json::Document document = json::parse(input);
    if (!parsed(document, &root, error))
        return false;
    bool ok = false;
    return json::boolean(*root, "ok", &ok) && ok;
}

bool parse_usb_drives(std::string_view input, std::vector<int> *out, std::string *error)
{
    const json::Value *root = nullptr;
    const json::Document document = json::parse(input);
    if (out == nullptr || !parsed(document, &root, error))
        return false;
    const json::Value *items = json::array(*root, "usb");
    if (items == nullptr || items->elements.size() > 8)
    {
        *error = "Invalid USB drive list";
        return false;
    }
    out->clear();
    for (const json::Value &item : items->elements)
    {
        if (item.type != json::NodeTag::number || item.number < 0 || item.number > 7 ||
            static_cast<double>(static_cast<int>(item.number)) != item.number)
        {
            *error = "Invalid USB drive number";
            return false;
        }
        const int drive = static_cast<int>(item.number);
        if (std::find(out->begin(), out->end(), drive) == out->end())
            out->push_back(drive);
    }
    return true;
}

bool parse_usb_export(std::string_view input, UsbExport *out, std::string *error)
{
    const json::Value *root = nullptr;
    const json::Document document = json::parse(input);
    if (out == nullptr || !parsed(document, &root, error))
        return false;
    if (json::has_error(*root, error))
        return false;
    if (!json::boolean(*root, "ok", &out->ok) || !out->ok)
    {
        *error = "Garlic rejected the USB export";
        return false;
    }
    if (!json::string(*root, "path", &out->path) || out->path.empty())
    {
        *error = "The USB export did not return a destination";
        return false;
    }
    (void)json::integer(*root, "copied", &out->copied);
    (void)json::integer(*root, "failed", &out->failed);
    if (out->failed < 0 || out->copied < -1)
    {
        *error = "The USB export returned invalid copy counts";
        return false;
    }
    return true;
}

bool parse_users(std::string_view input, std::vector<User> *out, std::string *error)
{
    const json::Value *root = nullptr;
    const json::Document document = json::parse(input);
    if (out == nullptr || !parsed(document, &root, error))
        return false;
    const json::Value *items = json::array(*root, "users");
    if (items == nullptr)
    {
        *error = "Missing users array";
        return false;
    }
    out->clear();
    for (const json::Value &item : items->elements)
    {
        User user;
        if (item.type != json::NodeTag::object || !json::string(item, "id", &user.id) ||
            !json::string(item, "name", &user.name) || import_path(0, user.id).empty())
        {
            *error = "Invalid user entry";
            return false;
        }
        out->push_back(std::move(user));
    }
    return true;
}

bool parse_import(std::string_view input, ImportResult *out, std::string *error)
{
    const json::Value *root = nullptr;
    const json::Document document = json::parse(input);
    if (out == nullptr || !parsed(document, &root, error))
        return false;
    if (json::has_error(*root, error))
        return false;
    bool ok = false;
    if (!json::boolean(*root, "ok", &ok) || !ok)
    {
        *error = "Garlic rejected the import";
        return false;
    }
    if (!json::string(*root, "title_id", &out->title_id) ||
        !json::string(*root, "dir_name", &out->dir_name))
    {
        *error = "The import did not return its destination";
        return false;
    }
    (void)json::string(*root, "main_title", &out->main_title);
    (void)json::string(*root, "warning", &out->warning);
    return true;
}

bool valid_elf(const std::vector<std::uint8_t> &data, std::string *error)
{
    auto fail = [&](const char *message)
    {
        if (error != nullptr)
            *error = message;
        return false;
    };
    if (data.size() < 64)
        return fail("The bundled Garlic payload is too short to be ELF64");
    if (data[0] != 0x7F || data[1] != 'E' || data[2] != 'L' || data[3] != 'F')
        return fail("The bundled Garlic payload is not an ELF image");
    if (data[4] != 2 || data[5] != 1)
        return fail("The Garlic payload must be little-endian ELF64");
    if (data[16] != 3 || data[17] != 0)
        return fail("The Garlic payload must be a position-independent ET_DYN image");
    if (data[18] != 62 || data[19] != 0)
        return fail("The Garlic payload must target x86-64");
    return true;
}

std::string mount_path(int index)
{
    return "/api/mount?idx=" + std::to_string(index);
}

std::string usb_export_path(UsbExportKind kind, int index, std::string_view title_id, int usb)
{
    if (usb < 0 || usb > 7)
        return {};
    const std::string drive = std::to_string(usb);
    switch (kind)
    {
    case UsbExportKind::decrypted_save:
        return index >= 0 ? "/api/dump_usb?idx=" + std::to_string(index) + "&usb=" + drive
                          : std::string();
    case UsbExportKind::encrypted_title:
        return title_id.empty()
                   ? std::string()
                   : "/api/dump_title_usb?id=" + percent_encode(title_id) + "&usb=" + drive;
    case UsbExportKind::decrypted_title:
        return title_id.empty()
                   ? std::string()
                   : "/api/dump_title_dec_usb?id=" + percent_encode(title_id) + "&usb=" + drive;
    }
    return {};
}

std::string import_path(int index, std::string_view account_id)
{
    if (index < 0 || account_id.empty() || account_id.size() > 16)
        return {};
    const bool hexadecimal = std::all_of(
        account_id.begin(), account_id.end(), [](unsigned char c)
        { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); });
    return hexadecimal ? "/api/import_usb_save?idx=" + std::to_string(index) +
                             "&uid=" + percent_encode(account_id)
                       : std::string();
}

std::string percent_encode(std::string_view value)
{
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    for (const unsigned char c : value)
    {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
        if (safe)
            result.push_back(static_cast<char>(c));
        else
        {
            result.push_back('%');
            result.push_back(digits[c >> 4]);
            result.push_back(digits[c & 15]);
        }
    }
    return result;
}

} // namespace garlic
