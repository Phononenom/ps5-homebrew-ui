// Garlic Save Companion - protocol and JSON tests.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "garlic/client.hpp"
#include "garlic/image.hpp"
#include "garlic/json.hpp"
#include "garlic/metadata.hpp"
#include "garlic/protocol.hpp"
#include "garlic/worker.hpp"
#include "ui/components/card.hpp"

#include <gtest/gtest.h>

#include "../../third_party/stb/stb_image.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace garlic::tests
{

TEST(Json, ParsesNestedValuesAndEscapes)
{
    const auto document = json::parse(
        R"({"name":"Garlic \u2605 \ud83d\ude00","ok":true,"values":[null,-12.5e2,"x\n"]})");
    ASSERT_TRUE(document.error.empty()) << document.error;
    ASSERT_EQ(document.root.type, json::NodeTag::object);
    std::string name;
    ASSERT_TRUE(json::string(document.root, "name", &name));
    EXPECT_EQ(name, "Garlic ★ 😀");
    bool ok = false;
    ASSERT_TRUE(json::boolean(document.root, "ok", &ok));
    EXPECT_TRUE(ok);
    const json::Value *values = json::array(document.root, "values");
    ASSERT_NE(values, nullptr);
    ASSERT_EQ(values->elements.size(), 3U);
    EXPECT_EQ(values->elements[0].type, json::NodeTag::null);
    EXPECT_DOUBLE_EQ(values->elements[1].number, -1250.0);
    EXPECT_EQ(values->elements[2].text, "x\n");
}

TEST(Json, RejectsMalformedInput)
{
    EXPECT_FALSE(json::parse("{").error.empty());
    EXPECT_FALSE(json::parse("[1,2").error.empty());
    EXPECT_FALSE(json::parse("{\"x\":01}").error.empty());
    EXPECT_FALSE(json::parse("\"\\uD800\"").error.empty());
    EXPECT_FALSE(json::parse("true false").error.empty());
}

TEST(Protocol, ParsesStatusAndSaves)
{
    const std::string statusJson =
        R"({"mounted":true,"mount_point":"/data/save_mnt/a","save_path":"/user/home/1/save"})";
    Status status;
    std::string error;
    ASSERT_TRUE(parse_status(statusJson, &status, &error)) << error;
    EXPECT_TRUE(status.mounted);
    EXPECT_EQ(status.mount_point, "/data/save_mnt/a");
    EXPECT_EQ(status.uid, "1");

    const std::string savesJson =
        R"({"saves":[{"title_id":"PPSA99137","save_name":"slot","path":"/save","dir":"slot","title_name":"Game","type":"ps5","backup":true,"usb":false,"uid":"10000000"}]})";
    std::vector<Save> saves;
    ASSERT_TRUE(parse_saves(savesJson, &saves, &error)) << error;
    ASSERT_EQ(saves.size(), 1U);
    EXPECT_EQ(saves[0].index, 0);
    EXPECT_EQ(saves[0].title_id, "PPSA99137");
    EXPECT_EQ(saves[0].save_name, "slot");
    EXPECT_EQ(saves[0].title_name, "Game");
    EXPECT_TRUE(saves[0].backup);
    EXPECT_FALSE(saves[0].usb);
}

TEST(Protocol, ParsesMountAndFiles)
{
    const std::string json =
        R"({"title_id":"PPSA99137","save_name":"slot","mount":"/mnt","account_id":"0x1","save_title":"Example","sfo_zeroed":false,"db_found":true,"files":[{"name":"sce_sys","dir":true,"size":0},{"name":"data.bin","dir":false,"size":4096}]})";
    Mount mount;
    std::string error;
    ASSERT_TRUE(parse_mount(json, &mount, &error)) << error;
    EXPECT_EQ(mount.save_title, "Example");
    ASSERT_EQ(mount.files.size(), 2U);
    EXPECT_TRUE(mount.files[0].directory);
    EXPECT_EQ(mount.files[1].size, 4096);

    std::vector<FileEntry> files;
    ASSERT_TRUE(parse_files("{\"files\":[]}", &files, &error)) << error;
    EXPECT_TRUE(files.empty());
}

TEST(Protocol, PreservesApiErrorsAndOperationResults)
{
    std::string error;
    EXPECT_FALSE(parse_operation("{\"error\":\"Mount failed\"}", &error));
    EXPECT_EQ(error, "Mount failed");
    Mount mount;
    EXPECT_FALSE(parse_mount("{\"error\":\"Mount failed\"}", &mount, &error));
    EXPECT_EQ(error, "Mount failed");
    EXPECT_TRUE(parse_operation("{\"ok\":true}", &error));
    EXPECT_FALSE(parse_operation("{\"ok\":false}", &error));
}

TEST(Protocol, ValidatesElfHeader)
{
    std::vector<std::uint8_t> data(64);
    data[0] = 0x7F;
    data[1] = 'E';
    data[2] = 'L';
    data[3] = 'F';
    data[4] = 2;
    data[5] = 1;
    data[16] = 3;
    data[18] = 62;
    std::string error;
    EXPECT_TRUE(valid_elf(data, &error)) << error;
    data[16] = 2;
    EXPECT_FALSE(valid_elf(data, &error));
    data = std::vector<std::uint8_t>(63);
    EXPECT_FALSE(valid_elf(data, &error));
}

TEST(Http, ParsesCompleteResponse)
{
    const std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                            "11\r\n\r\n{\"ok\":true}";
    const HttpResponse response = parse_http(raw);
    ASSERT_TRUE(response.ok) << response.error;
    EXPECT_EQ(response.status, 200);
    EXPECT_EQ(response.body, "{\"ok\":true}");
}

TEST(Http, RejectsLengthMismatchAndChunking)
{
    EXPECT_FALSE(parse_http("HTTP/1.1 200 OK\r\nContent-Length: 20\r\n\r\nshort").ok);
    EXPECT_FALSE(parse_http("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n").ok);
    EXPECT_FALSE(parse_http("not-http").ok);
}

TEST(Protocol, BuildsApiPathsAndEncodesTitles)
{
    EXPECT_EQ(mount_path(7), "/api/mount?idx=7");
}

TEST(Protocol, ParsesUsbDrivesAndExportResults)
{
    std::vector<int> drives;
    std::string error;
    ASSERT_TRUE(parse_usb_drives(R"({"usb":[0,2,7]})", &drives, &error)) << error;
    EXPECT_EQ(drives, (std::vector<int>{0, 2, 7}));
    ASSERT_TRUE(parse_usb_drives(R"({"usb":[]})", &drives, &error)) << error;
    EXPECT_TRUE(drives.empty());
    EXPECT_FALSE(parse_usb_drives(R"({"usb":[8]})", &drives, &error));
    EXPECT_FALSE(parse_usb_drives(R"({"usb":[1.5]})", &drives, &error));

    UsbExport exported;
    ASSERT_TRUE(parse_usb_export(
        R"({"ok":true,"copied":2,"failed":1,"path":"/mnt/usb0/PS5/SAVEDATA/GARLIC/PPSA12345"})",
        &exported, &error))
        << error;
    EXPECT_TRUE(exported.ok);
    EXPECT_EQ(exported.copied, 2);
    EXPECT_EQ(exported.failed, 1);
    EXPECT_EQ(exported.path, "/mnt/usb0/PS5/SAVEDATA/GARLIC/PPSA12345");
    EXPECT_FALSE(parse_usb_export(R"({"error":"Failed to copy files to USB"})", &exported, &error));
    EXPECT_EQ(error, "Failed to copy files to USB");
}

TEST(Protocol, BuildsOnlyExistingUsbExportPaths)
{
    EXPECT_EQ(usb_export_path(UsbExportKind::decrypted_save, 7, {}, 2),
              "/api/dump_usb?idx=7&usb=2");
    EXPECT_EQ(usb_export_path(UsbExportKind::encrypted_title, -1, "PPSA 123&X", 0),
              "/api/dump_title_usb?id=PPSA%20123%26X&usb=0");
    EXPECT_EQ(usb_export_path(UsbExportKind::decrypted_title, -1, "PPSA12345", 7),
              "/api/dump_title_dec_usb?id=PPSA12345&usb=7");
    EXPECT_TRUE(usb_export_path(UsbExportKind::encrypted_title, -1, "PPSA12345", 8).empty());
    EXPECT_TRUE(usb_export_path(UsbExportKind::decrypted_save, -1, {}, 0).empty());
}

TEST(Protocol, ParsesUsersAndImportResults)
{
    std::vector<User> users;
    std::string error;
    ASSERT_TRUE(parse_users(R"({"users":[{"id":"10000000","name":"Player"}]})", &users, &error))
        << error;
    ASSERT_EQ(users.size(), 1U);
    EXPECT_EQ(users[0].id, "10000000");
    EXPECT_EQ(users[0].name, "Player");
    EXPECT_FALSE(parse_users(R"({"users":[{"id":"bad","name":"Player"}]})", &users, &error));

    ImportResult result;
    ASSERT_TRUE(parse_import(
        R"({"ok":true,"title_id":"CUSA12345","dir_name":"slot-1","main_title":"Game","warning":"Save imported but DB update failed"})",
        &result, &error))
        << error;
    EXPECT_EQ(result.title_id, "CUSA12345");
    EXPECT_EQ(result.dir_name, "slot-1");
    EXPECT_EQ(result.main_title, "Game");
    EXPECT_EQ(result.warning, "Save imported but DB update failed");
    EXPECT_FALSE(parse_import(R"({"error":"Mount failed"})", &result, &error));
    EXPECT_EQ(error, "Mount failed");
}

TEST(Protocol, BuildsValidatedUsbImportPaths)
{
    EXPECT_EQ(import_path(7, "10000000"), "/api/import_usb_save?idx=7&uid=10000000");
    EXPECT_EQ(import_path(2, "ABCDEF12"), "/api/import_usb_save?idx=2&uid=ABCDEF12");
    EXPECT_TRUE(import_path(-1, "10000000").empty());
    EXPECT_TRUE(import_path(1, "").empty());
    EXPECT_TRUE(import_path(1, "not-hex").empty());
    EXPECT_TRUE(import_path(1, "10000000&force=1").empty());
}

TEST(Metadata, ParsesPassiveAddressAndValidatesTitlePaths)
{
    int port = 0;
    ASSERT_TRUE(parse_pasv_port("227 Entering Passive Mode (127,0,0,1,15,211)", &port));
    EXPECT_EQ(port, 4051);
    ASSERT_TRUE(parse_pasv_port("227 Entering Passive Mode (192,0,2,1,217,48).", &port));
    EXPECT_EQ(port, 55600);
    EXPECT_FALSE(parse_pasv_port("227 Entering Passive Mode (127,0,0,1,15,211) trailing", &port));
    EXPECT_FALSE(parse_pasv_port("227 Entering Passive Mode (127,0,0,1,0,0)", &port));
    EXPECT_FALSE(parse_pasv_port("227 Entering Passive Mode (127,0,0,1,999,1)", &port));
    EXPECT_FALSE(parse_pasv_port("200 OK", &port));
    EXPECT_EQ(title_photo_path("PPSA21564"), "/user/appmeta/PPSA21564/pic0.png");
    EXPECT_EQ(title_icon_path("PPSA21564"), "/user/appmeta/PPSA21564/icon0.png");
    EXPECT_TRUE(title_photo_path("PPSA21564/pic0.png").empty());
    EXPECT_TRUE(title_icon_path("PPSA21564/icon0.png").empty());
    EXPECT_TRUE(title_icon_path("short").empty());
}

TEST(Metadata, ArtworkJobsAreBackgroundWork)
{
    Worker worker;
    EXPECT_TRUE(worker.start({JobType::load_icon, -1, "PPSA21564"}));
    EXPECT_FALSE(worker.busy());
    worker.stop();
}

TEST(Image, DecodesPngForTextures)
{
    std::string data;
    ASSERT_TRUE(hui::save::read_file(HUI_SOURCE_DIR "/sce_sys/icon0.png", &data));
    int width = 0;
    int height = 0;
    int channels = 0;
    const auto *bytes = reinterpret_cast<const stbi_uc *>(data.data());
    stbi_uc *raw =
        stbi_load_from_memory(bytes, static_cast<int>(data.size()), &width, &height, &channels, 4);
    ASSERT_NE(raw, nullptr);
    Image image;
    std::string error;
    ASSERT_TRUE(decode_png(data, &image, &error)) << error;
    EXPECT_EQ(image.width, width);
    EXPECT_EQ(image.height, height);
    ASSERT_EQ(image.rgba.size(), static_cast<std::size_t>(width) * height * 4);
    const std::size_t stride = static_cast<std::size_t>(width) * 4;
    for (int y = 0; y < height; ++y)
    {
        EXPECT_EQ(std::memcmp(image.rgba.data() + static_cast<std::size_t>(y) * stride,
                              raw + static_cast<std::size_t>(y) * stride, stride),
                  0)
            << y;
    }
    stbi_image_free(raw);

    EXPECT_FALSE(decode_png("not a PNG", &image, &error));
    data[16] = 0;
    data[17] = 0;
    data[18] = 0x10;
    data[19] = 1;
    EXPECT_FALSE(decode_png(data, &image, &error));
    EXPECT_NE(error.find("dimensions"), std::string::npos);
}

TEST(Image, DownsamplesFourKLandscape)
{
    std::string data;
    ASSERT_TRUE(
        hui::save::read_file(HUI_SOURCE_DIR "/tests/fixtures/garlic-landscape-4k.png", &data));
    Image image;
    std::string error;
    ASSERT_TRUE(decode_png(data, &image, &error)) << error;
    EXPECT_EQ(image.width, 1024);
    EXPECT_EQ(image.height, 576);
    EXPECT_NEAR(static_cast<double>(image.width) / image.height, 16.0 / 9.0, 0.001);
    ASSERT_EQ(image.rgba.size(), 1024U * 576U * 4U);
    const auto pixel = [&](int x, int y)
    {
        const std::size_t offset = (static_cast<std::size_t>(y) * image.width + x) * 4;
        return std::vector<unsigned char>(image.rgba.begin() + offset,
                                          image.rgba.begin() + offset + 4);
    };
    EXPECT_EQ(pixel(0, 0), (std::vector<unsigned char>{34, 88, 131, 255}));
    EXPECT_EQ(pixel(0, 575), (std::vector<unsigned char>{20, 35, 57, 255}));
    EXPECT_EQ(pixel(1023, 575), (std::vector<unsigned char>{20, 35, 57, 255}));
}

TEST(Artwork, CoverFitCropsInsteadOfStretching)
{
    const hui::gfx::Rect square = hui::ui::cover_uv(hui::gfx::kFullUv, 1.0f, {0, 0, 1600, 900});
    EXPECT_FLOAT_EQ(square.x, 0.0f);
    EXPECT_FLOAT_EQ(square.w, 1.0f);
    EXPECT_FLOAT_EQ(square.y, 0.21875f);
    EXPECT_FLOAT_EQ(square.h, 0.5625f);
    const hui::gfx::Rect landscape =
        hui::ui::cover_uv(hui::gfx::kFullUv, 16.0f / 9.0f, {0, 0, 1000, 1000});
    EXPECT_FLOAT_EQ(landscape.x, 0.21875f);
    EXPECT_FLOAT_EQ(landscape.w, 0.5625f);
    EXPECT_FLOAT_EQ(landscape.y, 0.0f);
    EXPECT_FLOAT_EQ(landscape.h, 1.0f);
    const hui::gfx::Rect uncropped =
        hui::ui::cover_uv(hui::gfx::kFullUv, 16.0f / 9.0f, {0, 0, 1600, 900});
    EXPECT_FLOAT_EQ(uncropped.x, 0.0f);
    EXPECT_FLOAT_EQ(uncropped.y, 0.0f);
    EXPECT_FLOAT_EQ(uncropped.w, 1.0f);
    EXPECT_FLOAT_EQ(uncropped.h, 1.0f);
}

} // namespace garlic::tests
