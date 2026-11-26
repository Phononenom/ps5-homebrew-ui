// Garlic Save Companion - Garlic REST models and protocol helpers.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "garlic/json.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace garlic
{

struct Status
{
    bool mounted = false;
    std::string mount_point;
    std::string save_path;
    std::string uid;
};

struct Save
{
    int index = 0;
    std::string title_id;
    std::string save_name;
    std::string path;
    std::string dir;
    std::string title_name;
    std::string type;
    bool backup = false;
    bool usb = false;
    std::string uid;
};

struct FileEntry
{
    std::string name;
    bool directory = false;
    std::int64_t size = 0;
};

struct Mount
{
    std::string title_id;
    std::string save_name;
    std::string mount;
    std::string account_id;
    std::string save_title;
    bool sfo_zeroed = false;
    bool db_found = false;
    std::vector<FileEntry> files;
};

struct User
{
    std::string id;
    std::string name;
};

struct ImportResult
{
    std::string title_id;
    std::string dir_name;
    std::string main_title;
    std::string warning;
};

enum class UsbExportKind : std::uint8_t
{
    decrypted_save,
    encrypted_title,
    decrypted_title,
};

struct UsbExport
{
    bool ok = false;
    int copied = -1;
    int failed = 0;
    std::string path;
};

bool parse_status(std::string_view input, Status *out, std::string *error);
bool parse_saves(std::string_view input, std::vector<Save> *out, std::string *error);
bool parse_mount(std::string_view input, Mount *out, std::string *error);
bool parse_files(std::string_view input, std::vector<FileEntry> *out, std::string *error);
bool parse_operation(std::string_view input, std::string *error);
bool parse_usb_drives(std::string_view input, std::vector<int> *out, std::string *error);
bool parse_usb_export(std::string_view input, UsbExport *out, std::string *error);
bool parse_users(std::string_view input, std::vector<User> *out, std::string *error);
bool parse_import(std::string_view input, ImportResult *out, std::string *error);
bool valid_elf(const std::vector<std::uint8_t> &data, std::string *error);
std::string mount_path(int index);
std::string usb_export_path(UsbExportKind kind, int index, std::string_view title_id, int usb);
std::string import_path(int index, std::string_view account_id);
std::string percent_encode(std::string_view value);

} // namespace garlic
