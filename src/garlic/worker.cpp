// Garlic Save Companion - serialized Garlic API worker.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#include "garlic/worker.hpp"

#include "garlic/client.hpp"
#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"

#include <pthread.h>

namespace garlic
{
namespace
{

constexpr const char *kPayloadPath = "/app0/strings/payloads/garlic-savemgr.elf";

Result failure(JobType type, const std::string &message)
{
    Result result;
    result.type = type;
    result.error = message;
    return result;
}

bool successful(const HttpResponse &response, std::string *error)
{
    if (!response.error.empty())
    {
        *error = response.error;
        return false;
    }
    if (response.status < 200 || response.status >= 300)
    {
        *error = "Garlic returned HTTP " + std::to_string(response.status);
        return false;
    }
    return true;
}

} // namespace

Worker::Worker(Hooks hooks) : hooks_(std::move(hooks))
{
    joined_ = true;
    metadata_joined_ = true;
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0)
        return;
    (void)pthread_attr_setstacksize(&attributes, 1u << 20);
    if (pthread_create(
            &thread_, &attributes,
            [](void *arg) -> void *
            {
                static_cast<Worker *>(arg)->run();
                return nullptr;
            },
            this) == 0)
        joined_ = false;
    if (pthread_create(
            &metadata_thread_, &attributes,
            [](void *arg) -> void *
            {
                static_cast<Worker *>(arg)->run_metadata();
                return nullptr;
            },
            this) == 0)
        metadata_joined_ = false;
    (void)pthread_attr_destroy(&attributes);
}

Worker::~Worker()
{
    stop();
}

bool Worker::start(const Job &job)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (job.type == JobType::load_icon && !job.mounted_icon)
    {
        if (stopping_ || metadata_joined_)
            return false;
        metadata_jobs_.push_back(job);
        metadata_wake_.notify_one();
        return true;
    }
    if (stopping_ || joined_ || active_ || !jobs_.empty())
        return false;
    jobs_.push_back(job);
    wake_.notify_one();
    return true;
}

bool Worker::try_result(Result *out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (results_.empty())
        return false;
    *out = std::move(results_.front());
    results_.pop_front();
    return true;
}

bool Worker::busy() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return active_ || !jobs_.empty();
}

void Worker::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (joined_ && metadata_joined_)
            return;
        stopping_ = true;
        wake_.notify_all();
        metadata_wake_.notify_all();
    }
    if (!joined_)
        pthread_join(thread_, nullptr);
    if (!metadata_joined_)
        pthread_join(metadata_thread_, nullptr);
    joined_ = true;
    metadata_joined_ = true;
}

void Worker::run()
{
    for (;;)
    {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [&] { return stopping_ || !jobs_.empty(); });
            if (stopping_)
                break;
            job = jobs_.front();
            jobs_.pop_front();
            active_ = true;
        }

        Result result = execute(job);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            results_.push_back(std::move(result));
            active_ = false;
        }
    }
    unmount_if_needed();
}

void Worker::run_metadata()
{
    for (;;)
    {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            metadata_wake_.wait(lock, [&] { return stopping_ || !metadata_jobs_.empty(); });
            if (stopping_)
                break;
            job = metadata_jobs_.front();
            metadata_jobs_.pop_front();
        }
        Result result;
        result.type = JobType::load_icon;
        result.title_id = job.title_id;
        result.save_name = job.save_name;
        if (!fetch_title_icon(job.title_id, &result.image, &result.error))
            hui::sys::log("[Garlic] metadata artwork unavailable for %s: %s", job.title_id.c_str(),
                          result.error.c_str());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            results_.push_back(std::move(result));
        }
    }
}

Result Worker::execute(const Job &job)
{
    switch (job.type)
    {
    case JobType::startup:
        return startup();
    case JobType::refresh_saves:
        return saves(job.type, "Save list refreshed");
    case JobType::mount:
    {
        HttpResponse response = get(mount_path(job.index));
        std::string error;
        if (!successful(response, &error))
            return failure(job.type, error);
        Mount mount;
        if (!parse_mount(response.body, &mount, &error))
            return failure(job.type, error);
        mounted_by_app_ = true;
        Result result;
        result.type = job.type;
        result.mount = std::move(mount);
        result.files = result.mount.files;
        result.message = "Save mounted";
        return result;
    }
    case JobType::refresh_files:
        return files();
    case JobType::unmount:
    {
        HttpResponse response = get("/api/unmount");
        std::string error;
        if (!successful(response, &error) || !parse_operation(response.body, &error))
            return failure(job.type, error.empty() ? "Unmount failed" : error);
        mounted_by_app_ = false;
        Result result;
        result.type = job.type;
        result.message = "Save unmounted";
        return result;
    }
    case JobType::refresh_usb:
        return usb_drives();
    case JobType::usb_export:
        return export_usb(job);
    case JobType::load_icon:
        return icon(job);
    case JobType::prepare_import:
        return prepare_import();
    case JobType::import_save:
        return import_save(job);
    }
    return failure(job.type, "Unknown Garlic job");
}

Result Worker::startup()
{
    const auto now = [this] { return hooks_.now ? hooks_.now() : hui::sys::monotonic_us(); };
    const auto pause = [this](std::int64_t duration)
    {
        if (hooks_.sleep)
            hooks_.sleep(duration);
        else
            hui::sys::sleep_us(static_cast<uint32_t>(duration));
    };
    bool started = false;
    Status status;
    if (!healthy(&status))
    {
        hui::sys::log("[Garlic] startup: API unavailable; reading bundled ELF");
        std::string data;
        const bool read = hooks_.read ? hooks_.read(kPayloadPath, &data)
                                      : hui::save::read_file(kPayloadPath, &data);
        if (!read)
            return failure(JobType::startup, "Could not read the bundled Garlic payload");
        std::vector<std::uint8_t> bytes(data.begin(), data.end());
        std::string error;
        if (!valid_elf(bytes, &error))
            return failure(JobType::startup, error);
        hui::sys::log("[Garlic] startup: sending %zu bytes to 127.0.0.1:9021", bytes.size());
        const std::int64_t deadline = now() + 20'000'000;
        std::string last_error;
        while (!started && !healthy(&status) && now() < deadline)
        {
            const bool delivered =
                hooks_.deliver ? hooks_.deliver(bytes, &error) : send_payload(bytes, &error);
            if (delivered)
            {
                started = true;
                break;
            }
            last_error = error;
            hui::sys::log("[Garlic] startup: payload send failed: %s; waiting for loader",
                          error.c_str());
            pause(250'000);
        }
        if (!started && !healthy(&status))
        {
            if (last_error.empty())
                last_error = "Could not deliver the Garlic payload to 127.0.0.1:9021";
            return failure(JobType::startup, last_error);
        }
        if (started)
        {
            hui::sys::log("[Garlic] startup: payload sent; waiting for API on 8082");
            const std::int64_t api_deadline = now() + 20'000'000;
            while (!healthy(&status))
            {
                if (now() >= api_deadline)
                    return failure(JobType::startup,
                                   "Garlic was sent, but its API did not start on port 8082");
                pause(250'000);
            }
            hui::sys::log("[Garlic] startup: API is healthy");
        }
        else
        {
            hui::sys::log("[Garlic] startup: API became healthy; no duplicate payload sent");
        }
    }
    else
    {
        hui::sys::log("[Garlic] startup: reusing healthy API on 8082; no payload sent");
    }
    Result result = saves(JobType::startup, "Garlic is ready");
    if (!result.error.empty())
        return result;
    result.started_daemon = started;
    result.daemon_mounted = status.mounted;
    result.account_id = status.uid;
    HttpResponse users_response = get("/api/users");
    std::string error;
    if (!successful(users_response, &error) ||
        !parse_users(users_response.body, &result.users, &error))
        hui::sys::log("[Garlic] startup: account discovery failed: %s", error.c_str());
    return result;
}

Result Worker::saves(JobType type, const char *message)
{
    HttpResponse response = get("/api/saves");
    std::string error;
    if (!successful(response, &error))
        return failure(type, error);
    Result result;
    result.type = type;
    if (!parse_saves(response.body, &result.saves, &error))
        return failure(type, error);
    result.message = message;
    return result;
}

Result Worker::files()
{
    HttpResponse response = get("/api/files");
    std::string error;
    if (!successful(response, &error))
        return failure(JobType::refresh_files, error);
    Result result;
    result.type = JobType::refresh_files;
    if (!parse_files(response.body, &result.files, &error))
        return failure(result.type, error);
    result.message = "File list refreshed";
    return result;
}

Result Worker::usb_drives()
{
    Result result;
    result.type = JobType::refresh_usb;
    HttpResponse response = get("/api/usb_list");
    std::string error;
    if (!successful(response, &error) ||
        !parse_usb_drives(response.body, &result.usb_drives, &error))
        return failure(result.type, error.empty() ? "Could not check USB drives" : error);
    return result;
}

Result Worker::export_usb(const Job &job)
{
    const bool decrypted = job.export_kind != UsbExportKind::encrypted_title;
    const std::string path = usb_export_path(job.export_kind, job.index, job.title_id, job.usb);
    if (path.empty())
        return failure(job.type, "Invalid USB export request");
    HttpResponse response = get(path, 15 * 60);
    if (decrypted)
        mounted_by_app_ = false;
    Result result;
    result.type = job.type;
    result.mount_released = decrypted;
    result.title_id = job.title_id;
    result.save_name = job.save_name;
    std::string error;
    if (!successful(response, &error) ||
        !parse_usb_export(response.body, &result.export_result, &error))
    {
        result.error = error.empty() ? "USB export failed" : error;
        return result;
    }
    result.message = result.export_result.failed > 0 ? "USB export finished with failures"
                                                     : "USB export complete";
    return result;
}

Result Worker::icon(const Job &job)
{
    Result result;
    result.type = job.type;
    result.title_id = job.title_id;
    result.save_name = job.save_name;
    result.mounted_icon = job.mounted_icon;
    std::string error;
    if (job.mounted_icon)
    {
        HttpResponse response = get("/api/icon");
        if (successful(response, &error) && decode_png(response.body, &result.image, &error))
            return result;
    }
    if (!fetch_title_icon(job.title_id, &result.image, &error))
        result.error = error;
    return result;
}

Result Worker::prepare_import()
{
    std::vector<User> users;
    HttpResponse response = get("/api/users");
    std::string error;
    if (!successful(response, &error) || !parse_users(response.body, &users, &error))
        return failure(JobType::prepare_import,
                       error.empty() ? "Could not read PS5 accounts" : error);
    Result result = saves(JobType::prepare_import, "");
    result.users = std::move(users);
    return result;
}

Result Worker::import_save(const Job &job)
{
    const std::string path = import_path(job.index, job.account_id);
    if (path.empty())
        return failure(job.type, "Invalid USB import request");
    HttpResponse response = get(path, 15 * 60);
    Result result;
    result.type = job.type;
    Status status;
    if (healthy(&status))
    {
        result.mount_released = !status.mounted;
        if (!status.mounted)
            mounted_by_app_ = false;
    }
    else
    {
        result.mount_released = true;
        mounted_by_app_ = false;
    }
    result.title_id = job.title_id;
    result.save_name = job.save_name;
    std::string error;
    if (!successful(response, &error) ||
        !parse_import(response.body, &result.import_result, &error))
    {
        result.error = error.empty() ? "USB import failed" : error;
        return result;
    }
    Result refreshed = saves(JobType::import_save, "");
    if (refreshed.error.empty())
        result.saves = std::move(refreshed.saves);
    else if (!result.import_result.warning.empty())
        result.import_result.warning += "; " + refreshed.error;
    else
        result.import_result.warning = refreshed.error;
    result.message = "Save imported";
    return result;
}

bool Worker::healthy(Status *status)
{
    if (hooks_.healthy)
        return hooks_.healthy(status);
    HttpResponse response = get("/api/status");
    if (!successful(response, &response.error))
        return false;
    std::string error;
    return parse_status(response.body, status, &error);
}

void Worker::unmount_if_needed()
{
    if (!mounted_by_app_)
        return;
    (void)get("/api/unmount");
    mounted_by_app_ = false;
}

} // namespace garlic
