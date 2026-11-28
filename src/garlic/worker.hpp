// Garlic Save Companion - serialized Garlic API worker.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "garlic/image.hpp"
#include "garlic/metadata.hpp"
#include "garlic/protocol.hpp"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace garlic
{

enum class JobType : std::uint8_t
{
    startup,
    refresh_saves,
    mount,
    refresh_files,
    unmount,
    refresh_usb,
    usb_export,
    load_icon,
    prepare_import,
    import_save,
};

struct Job
{
    Job(JobType kind = JobType::startup, int save_index = -1, std::string title = {},
        int drive = -1, UsbExportKind export_kind = UsbExportKind::decrypted_save,
        std::string save = {}, std::string account = {}, bool mounted_icon = false)
        : type(kind), index(save_index), title_id(std::move(title)), usb(drive),
          export_kind(export_kind), save_name(std::move(save)), account_id(std::move(account)),
          mounted_icon(mounted_icon)
    {
    }

    JobType type = JobType::startup;
    int index = -1;
    std::string title_id;
    int usb = -1;
    UsbExportKind export_kind = UsbExportKind::decrypted_save;
    std::string save_name;
    std::string account_id;
    bool mounted_icon = false;
};

struct Result
{
    JobType type = JobType::startup;
    bool started_daemon = false;
    bool daemon_mounted = false;
    bool mount_released = false;
    bool mounted_icon = false;
    std::vector<Save> saves;
    Mount mount;
    std::vector<FileEntry> files;
    std::vector<int> usb_drives;
    UsbExport export_result;
    Image image;
    std::vector<User> users;
    ImportResult import_result;
    std::string account_id;
    std::string title_id;
    std::string save_name;
    std::string message;
    std::string error;
};

class Worker
{
  public:
    struct Hooks
    {
        std::function<bool(Status *)> healthy;
        std::function<bool(const std::vector<std::uint8_t> &, std::string *)> deliver;
        std::function<bool(const std::string &, std::string *)> read;
        std::function<std::int64_t()> now;
        std::function<void(std::int64_t)> sleep;
    };

    explicit Worker(Hooks hooks = {});
    ~Worker();
    Worker(const Worker &) = delete;
    Worker &operator=(const Worker &) = delete;

    bool start(const Job &job);
    bool try_result(Result *out);
    bool busy() const;
    void stop();

  private:
    void run();
    void run_metadata();
    Result execute(const Job &job);
    Result startup();
    Result saves(JobType type, const char *message);
    Result files();
    Result usb_drives();
    Result export_usb(const Job &job);
    Result icon(const Job &job);
    Result prepare_import();
    Result import_save(const Job &job);
    bool healthy(Status *status);
    void unmount_if_needed();

    Hooks hooks_;
    std::deque<Job> jobs_;
    std::deque<Job> metadata_jobs_;
    std::deque<Result> results_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable metadata_wake_;
    bool stopping_ = false;
    bool active_ = false;
    bool mounted_by_app_ = false;
    bool joined_ = false;
    bool metadata_joined_ = false;
    pthread_t thread_{};
    pthread_t metadata_thread_{};
};

} // namespace garlic
