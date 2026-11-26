// Garlic Save Companion - a console-grade Garlic SaveMgr front end.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/concepts.hpp"

#include "core/save_file.hpp"
#include "garlic/worker.hpp"
#include "platform/ps5/system.hpp"
#include "ui/components.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace hui::concepts
{
namespace
{

using gfx::Color;
using gfx::Rect;

constexpr float kMargin = 96.0f;
constexpr float kRight = 1824.0f;
const Color kInk = Color::rgb(0xf2f4ff);
const Color kMuted = Color::rgb(0xa6b0ca);
const Color kAccent = Color::rgb(0xf0c75e);
const Color kBlue = Color::rgb(0x74c7ee);

[[maybe_unused]] constexpr app::TourStep kTour[] = {
    {0.5f, action_bit(Action::page_next)},
    {0.5f, 0, Direction::none, "other-user"},
    {0.5f, action_bit(Action::page_prev)},
    {0.5f, 0, Direction::none, "test-user"},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "title-saves"},
    {0.5f, 0, Direction::right},
    {0.5f, 0, Direction::none, "selection"},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "details"},
    {0.5f, action_bit(Action::confirm)},
    {0.6f, 0, Direction::none, "files"},
    {0.4f, action_bit(Action::north)},
    {0.6f, 0, Direction::none, "files-refreshed"},
    {0.5f, action_bit(Action::west)},
    {0.6f, 0, Direction::none, "unmounted"},
    {0.5f, action_bit(Action::confirm)},
    {0.6f, 0, Direction::none, "artwork"},
    {0.3f, 0, Direction::down},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "usb-options"},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "usb-drives"},
    {0.5f, action_bit(Action::confirm)},
    {0.2f, 0, Direction::none, "usb-exporting"},
    {1.2f, 0, Direction::none, "usb-decrypted"},
    {0.5f, action_bit(Action::back)},
    {0.3f, 0, Direction::down},
    {0.5f, action_bit(Action::confirm)},
    {0.3f, 0, Direction::down},
    {0.5f, action_bit(Action::confirm)},
    {0.4f, action_bit(Action::confirm)},
    {1.2f, 0, Direction::none, "usb-encrypted"},
    {0.5f, action_bit(Action::back)},
    {0.5f, action_bit(Action::back)},
    {0.5f, action_bit(Action::back), Direction::none, "return-library"},
    {0.5f, action_bit(Action::west)},
    {0.5f, 0, Direction::none, "usb-titles"},
    {0.5f, 0, Direction::right},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "usb-saves"},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "usb-source"},
    {0.3f, 0, Direction::down},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "import-account"},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "import-confirm"},
    {0.2f, 0, Direction::right},
    {0.5f, action_bit(Action::confirm)},
    {0.2f, 0, Direction::none, "importing"},
    {1.2f, 0, Direction::none, "import-complete"},
    {0.5f, action_bit(Action::confirm)},
    {0.3f, 0, Direction::down},
    {0.5f, action_bit(Action::confirm)},
    {0.4f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "import-overwrite"},
    {0.5f, action_bit(Action::back)},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::down},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "import-other-account-overwrite"},
    {0.5f, action_bit(Action::back)},
    {0.5f, action_bit(Action::back)},
    {0.5f, 0, Direction::none, "import-cancelled"},
};

[[maybe_unused]] constexpr app::TourStep kConsoleTour[] = {
    {0.5f, 0, Direction::none, "titles"},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "title-saves"},
    {0.5f, 0, Direction::right},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "details"},
    {0.5f, 0, Direction::down},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "usb-options"},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "usb-drives"},
    {0.5f, action_bit(Action::back)},
    {0.5f, action_bit(Action::back)},
    {0.5f, action_bit(Action::back)},
    {0.5f, action_bit(Action::west)},
    {0.5f, 0, Direction::none, "usb-titles"},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "usb-saves"},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "usb-details"},
    {0.5f, 0, Direction::down},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "import-account"},
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::none, "import-confirm"},
    {0.5f, action_bit(Action::back)},
    {0.5f, action_bit(Action::back)},
    {0.5f, 0, Direction::none, "import-cancelled"},
};

enum class Page : std::uint8_t
{
    loading,
    library,
    saves,
    details,
    files,
    exporting,
    importing,
    error,
};

struct TitleEntry
{
    std::string id;
    std::string name;
    std::string type;
    int console_saves = 0;
    int usb_saves = 0;
};

const char *kTechniques[] = {
    "Title-grouped console and USB libraries with landscape game artwork",
    "Per-title metadata loaded off thread; mounted icons override individual saves",
    "Decrypted and encrypted USB exports, with account-selected import and safe confirmation",
    "A sortable file table backed by Garlic's live mount API and bounded background workers",
};

std::string display_title(const garlic::Save &save)
{
    return save.title_name.empty() ? save.title_id : save.title_name;
}

std::string format_size(std::int64_t bytes)
{
    char text[64];
    if (bytes < 1024)
        std::snprintf(text, sizeof(text), "%lld B", static_cast<long long>(bytes));
    else if (bytes < 1024 * 1024)
        std::snprintf(text, sizeof(text), "%.1f KiB", bytes / 1024.0);
    else if (bytes < 1024 * 1024 * 1024)
        std::snprintf(text, sizeof(text), "%.1f MiB", bytes / (1024.0 * 1024.0));
    else
        std::snprintf(text, sizeof(text), "%.1f GiB", bytes / (1024.0 * 1024.0 * 1024.0));
    return text;
}

std::string uppercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::string spaced_path(std::string_view path)
{
    std::string result;
    result.reserve(path.size() + 12);
    for (const char c : path)
    {
        result.push_back(c);
        if (c == '/')
            result.push_back(' ');
    }
    return result;
}

ui::DetailItem detail(std::string label, std::string value, bool header = false)
{
    return {std::move(label), std::move(value), header, false, false, {}, 0};
}

ui::ListItem export_row(std::string title, std::string subtitle, int tag, bool header = false,
                        bool disabled = false)
{
    return {std::move(title), std::move(subtitle), {}, {}, {}, false, disabled, header, tag};
}

void garlic_mark(gfx::DrawList &list, float cx, float cy, float size, Color color,
                 std::uint32_t texture = 0)
{
    if (texture != 0)
    {
        list.image(texture, {cx - size * 0.5f, cy - size * 0.5f, size, size}, gfx::kFullUv,
                   Color::rgb(0xffffff), size * 0.12f);
        return;
    }
    list.line(cx, cy - size * 0.08f, cx + size * 0.15f, cy - size * 0.46f, size * 0.085f, color);
    list.line(cx + size * 0.07f, cy - size * 0.29f, cx - size * 0.14f, cy - size * 0.43f,
              size * 0.065f, color);
    list.rounded_rect({cx - size * 0.36f, cy - size * 0.13f, size * 0.31f, size * 0.53f},
                      size * 0.16f, color.with_alpha(0.68f));
    list.rounded_rect({cx + size * 0.05f, cy - size * 0.13f, size * 0.31f, size * 0.53f},
                      size * 0.16f, color.with_alpha(0.68f));
    list.rounded_rect({cx - size * 0.22f, cy - size * 0.18f, size * 0.44f, size * 0.63f},
                      size * 0.22f, color);
    list.line(cx - size * 0.2f, cy + size * 0.43f, cx - size * 0.25f, cy + size * 0.51f,
              size * 0.035f, color);
    list.line(cx, cy + size * 0.43f, cx, cy + size * 0.53f, size * 0.035f, color);
    list.line(cx + size * 0.2f, cy + size * 0.43f, cx + size * 0.25f, cy + size * 0.51f,
              size * 0.035f, color);
}

class Garlic final : public app::Concept
{
  public:
    explicit Garlic(app::Context &context) : context_(context)
    {
        theme_ = ui::themes().front();
        theme_.id = "garlic";
        theme_.name = "Garlic";
        theme_.family = "Native save tools";
        theme_.summary = "A focused save library with live detail and backup actions";
        theme_.page = Color::rgb(0x0d1020);
        theme_.surface = Color::rgb(0x1a2138);
        theme_.surface_high = Color::rgb(0x293451);
        theme_.text = kInk;
        theme_.text_muted = kMuted;
        theme_.primary = kAccent;
        theme_.on_primary = Color::rgb(0x171307);
        theme_.accent = kBlue;
        theme_.focus = kAccent;

        loader_.style.theme = theme_;
        loader_.style.indicator = ui::LoadingIndicator::spinner;
        loader_.title = "Garlic Save Companion";
        loader_.subtitle = "Connecting to the local save service";
        loader_.set_stages({{"Checking Garlic", 1}, {"Reading your save library", 1}});
        loader_.set_tips({"Cross opens a title, then a save. Circle goes back.",
                          "LB and RB switch PS5 accounts. Square switches console and USB saves.",
                          "Only saves mounted by this app are unmounted on exit."});

        grid_.style.theme = theme_;
        grid_.style.columns = 3;
        grid_.style.card.art_aspect = 16.0f / 9.0f;
        grid_.style.card.glow = true;
        grid_.style.card.badge_corner = ui::CardCorner::top_right;
        grid_.set_bounds({kMargin, 250, 1008, 670});
        grid_.art = [this](ui::Canvas &canvas, const Rect &art, float radius,
                           const ui::CardItem &item, float focus)
        {
            canvas.list.gradient_rect(art, radius, item.top, item.bottom);
            canvas.list.circle(art.cx(), art.cy() - 12, 75 + focus * 5,
                               Color::rgb(0xffffff, 0.045f));
            if (item.texture != 0)
            {
                canvas.list.image(item.texture, art,
                                  ui::cover_uv(gfx::kFullUv, item.image_aspect, art),
                                  Color::rgb(0xffffff), radius);
            }
            else
            {
                garlic_mark(canvas.list, art.cx(), art.cy() - 8, 104, kAccent.with_alpha(0.78f),
                            logo_texture_);
            }
            ui::text(canvas.list, canvas.fonts.semibold,
                     page_ == Page::library ? "TITLE" : "SAVE DATA", art.x + 22, art.y + art.h - 22,
                     16, kInk.with_alpha(0.64f), gfx::Align::left, 2.0f);
        };

        preview_.style.theme = theme_;
        preview_.style.panel = false;
        preview_.style.row_height = 50;
        preview_.style.label_size = 20;
        preview_.style.value_size = 24;
        preview_.set_bounds({1184, 458, 616, 284});
        preview_.set_active(false);

        details_.style.theme = theme_;
        details_.style.row_height = 43;
        details_.style.label_size = 20;
        details_.style.value_size = 23;
        details_.set_bounds({1160, 250, 664, 370});
        details_.set_active(false);

        actions_.style.theme = theme_;
        actions_.style.layout = ui::GroupLayout::column;
        actions_.style.height = 72;
        actions_.style.text_size = 24;
        actions_.style.gap = 16;
        actions_.set_bounds({1160, 728, 664, 168});

        files_.style.theme = theme_;
        files_.set_columns({
            {"Name", 0, 4.0f, gfx::Align::left, true, true, true, {}},
            {"Kind", 180, 1.0f, gfx::Align::left, false, false, true, {}},
            {"Size", 220, 1.0f, gfx::Align::right, true, false, true, {}},
        });
        files_.set_bounds({kMargin, 370, 1728, 548});

        empty_.style.theme = theme_;
        empty_.style.panel = true;
        empty_.style.float_icon = false;
        empty_.set_bounds({616, 360, 688, 400});
        empty_.title = "No saves found";
        empty_.body = "Play a game to create a save, then refresh your library.";
        empty_.action = "Refresh library";

        error_.style.theme = theme_;
        error_.style.panel = true;
        error_.style.float_icon = false;
        error_.set_bounds({616, 340, 688, 430});
        error_.action = "Try again";

        crumbs_.style.theme = theme_;
        crumbs_.set_bounds({kMargin, 172, 1200, 44});

        toasts_.style.theme = theme_;
        toasts_.style.anchor = ui::ToastAnchor::top_right;
        toasts_.style.margin = kMargin;
        toasts_.style.width = 560;
        toasts_.style.body_lines = 3;
        toasts_.set_bounds({0, 100, gfx::kVirtualWidth, 860});

        export_sheet_.style.theme = theme_;
        export_sheet_.style.size = 720;
        export_sheet_.style.footer = 96;
        export_sheet_.set_title("Export save data to USB");
        export_options_.style.theme = theme_;
        export_options_.style.row_height = 76;
        export_options_.style.title_size = 26;
        export_options_.style.subtitle_size = 20;
        export_options_.set_bounds(export_sheet_.content_rect());
        export_options_.set_active(false);
        export_sheet_.content = [this](ui::Canvas &canvas, const Rect &area, float)
        {
            export_options_.draw(canvas);
            ui::text(canvas.list, canvas.fonts.regular,
                     "Decrypted exports unmount the current save.", area.x, area.y + area.h - 20,
                     21, kMuted);
        };

        account_sheet_.style.theme = theme_;
        account_sheet_.style.size = 640;
        account_sheet_.style.footer = 96;
        account_sheet_.set_title("Import from USB");
        account_users_.style.theme = theme_;
        account_users_.style.row_height = 76;
        account_users_.style.title_size = 26;
        account_users_.style.subtitle_size = 20;
        account_users_.set_bounds(account_sheet_.content_rect());
        account_users_.set_active(false);
        account_sheet_.content = [this](ui::Canvas &canvas, const Rect &area, float)
        {
            account_users_.draw(canvas);
            ui::text(canvas.list, canvas.fonts.regular,
                     "Garlic sets save ownership for this account.", area.x, area.y + area.h - 20,
                     21, kMuted);
        };
        import_dialog_.style.theme = theme_;
        import_dialog_.style.width = 820;
        import_dialog_.style.centered = false;
        import_dialog_.style.body_lines = 8;

        palette_[0].snap(Color::rgb(0x0b1026));
        palette_[1].snap(Color::rgb(0x241548));
        palette_[2].snap(Color::rgb(0x1a4d73));
        palette_[3].snap(Color::rgb(0x744a96));
        loader_.art = [this](ui::Canvas &canvas, const Rect &screen, float)
        { garlic_mark(canvas.list, screen.cx(), screen.cy() - 48, 256, kAccent, logo_texture_); };
        load_logo();
    }

    ~Garlic() override
    {
        clear_artwork();
        if (logo_texture_ != 0 && context_.texture_destructor)
            context_.texture_destructor(logo_texture_);
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "garlic",
            "Garlic Saves",
            "Your PS5 save library, with a controller in mind",
            "src/concepts/garlic.cpp",
            audio::SoundSet::glass,
            kAccent,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        page_age_ = 0.0f;
        mounted_by_app_ = false;
        daemon_mounted_ = false;
        quitting_ = false;
        source_usb_ = false;
        selected_ = 0;
        selected_title_index_ = 0;
        selected_title_.clear();
        titles_.clear();
        all_saves_.clear();
        saves_.clear();
        results_.clear();
        pending_artwork_.clear();
        fixture_delay_ = 0.0f;
        export_drives_ = false;
        export_sheet_.dismiss();
        account_sheet_.dismiss();
        import_dialog_.dismiss();
        loader_.title = "Garlic Save Companion";
        loader_.subtitle = "Connecting to the local save service";
        loader_.set_stages({{"Checking Garlic", 1}, {"Reading your save library", 1}});
        set_page(Page::loading);
        loader_.show(feedback_placeholder_);
        start({garlic::JobType::startup, -1, {}});
#if defined(__linux__)
        garlic::Result startup;
        startup.type = garlic::JobType::startup;
        startup.saves = fixtures();
        startup.users = {{"10000000", "Test User"}, { "20000000", "Other User" }};
        startup.account_id = "10000000";
        startup.started_daemon = true;
        results_.push_back(std::move(startup));
#endif
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        clock_ += dt;
        page_age_ += dt;
        poll_results(dt, feedback);
        loader_.update(dt);
        grid_.update(dt);
        preview_.update(dt);
        details_.update(dt);
        actions_.update(dt);
        files_.update(dt);
        empty_.update(dt);
        error_.update(dt);
        crumbs_.update(dt);
        toasts_.update(dt, feedback);
        export_sheet_.update(dt);
        export_options_.update(dt);
        account_sheet_.update(dt);
        account_users_.update(dt);
        import_dialog_.update(dt);
        for (ui::SpringColor &color : palette_)
            color.update(dt, context_.settings.reduced_motion ? 40.0f : 4.0f);
        sync_actions();

        if (quitting_)
        {
#if !defined(__linux__)
            if (!worker_.busy())
            {
                worker_.stop();
                hui::sys::quit();
            }
#endif
            return;
        }
        if (input.is_pressed(Action::menu))
        {
            begin_quit(feedback);
            return;
        }
        if (input.is_pressed(Action::page_prev) || input.is_pressed(Action::page_next))
        {
            const int direction = input.is_pressed(Action::page_next) ? 1 : -1;
            if (account_sheet_.is_open())
                cycle_account(direction, feedback);
            else if ((page_ == Page::library || page_ == Page::saves) && !export_sheet_.is_open() &&
                     !import_dialog_.is_open())
                cycle_user(direction, feedback);
            return;
        }
        if (import_dialog_.is_open())
        {
            if (import_dialog_.handle(input, feedback) == ui::Event::activated &&
                import_dialog_.choice() == 1)
                confirm_import();
            return;
        }
        if (account_sheet_.is_open())
        {
            if (account_sheet_.handle(input, feedback) == ui::Event::none &&
                account_users_.handle(input, feedback) == ui::Event::activated)
                choose_import_account(account_users_.focus(), feedback);
            return;
        }
        if (export_sheet_.is_open())
        {
            if (export_sheet_.handle(input, feedback) == ui::Event::none &&
                export_options_.handle(input, feedback) == ui::Event::activated)
                choose_export(export_options_.focus());
            return;
        }
        if (page_ == Page::loading || page_ == Page::exporting || page_ == Page::importing)
        {
            (void)loader_.handle(input, feedback);
            return;
        }
        if (page_ == Page::error)
        {
            if (input.is_pressed(Action::confirm))
                enter();
            else if (input.is_pressed(Action::back))
                begin_quit(feedback);
            return;
        }
        if (input.is_pressed(Action::back))
        {
            feedback.play(audio::Cue::back);
            if (page_ == Page::library)
                begin_quit(feedback);
            else if (page_ == Page::saves)
                set_page(Page::library);
            else if (page_ == Page::files)
                set_page(Page::details);
            else
                set_page(Page::saves);
            return;
        }
        if (input.is_pressed(Action::north))
        {
            if (page_ == Page::library || page_ == Page::saves)
                start({garlic::JobType::refresh_saves, -1, {}});
            else if (page_ == Page::files)
                start({garlic::JobType::refresh_files, -1, {}});
        }
        if (input.is_pressed(Action::west))
        {
            if (page_ == Page::files)
                start({garlic::JobType::unmount, -1, {}});
            else if (page_ == Page::library || page_ == Page::saves)
                cycle_source(feedback);
            return;
        }

        switch (page_)
        {
        case Page::library:
            if (grid_.handle(input, feedback) == ui::Event::activated)
                open_title(feedback);
            else if (grid_.count() == 0 && input.is_pressed(Action::confirm))
                start({garlic::JobType::refresh_saves, -1, {}});
            if (page_ == Page::library && grid_.focus() != selected_title_index_)
            {
                selected_title_index_ = grid_.focus();
                if (const TitleEntry *title = selected_title())
                    selected_title_ = title->id;
                update_preview();
                update_palette();
            }
            break;
        case Page::saves:
            if (grid_.handle(input, feedback) == ui::Event::activated)
                open_details(feedback);
            else if (saves_.empty() && input.is_pressed(Action::confirm))
                start({garlic::JobType::refresh_saves, -1, {}});
            if (grid_.focus() != selected_)
            {
                selected_ = grid_.focus();
                update_preview();
                update_palette();
            }
            break;
        case Page::details:
            if (actions_.handle(input, feedback) == ui::Event::activated)
                run_action(actions_.focus(), feedback);
            break;
        case Page::files:
            files_.handle(input, feedback);
            break;
        case Page::loading:
        case Page::exporting:
        case Page::importing:
        case Page::error:
            break;
        }
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::gradient;
        for (int i = 0; i < 4; ++i)
            frame.backdrop.colors[i] = palette_[i].value();
        frame.backdrop.time = context_.settings.reduced_motion ? 0.0f : clock_;
        gfx::DrawList &list = frame.scene;
        ui::Canvas scene{list, context_.fonts, frame.glass_texture, clock_};

        if (page_ == Page::loading || page_ == Page::exporting || page_ == Page::importing ||
            quitting_)
        {
            loader_.draw(scene);
            toasts_.draw(scene);
            return;
        }
        draw_header(list);
        crumbs_.draw(scene);

        switch (page_)
        {
        case Page::library:
        case Page::saves:
            if (grid_.count() == 0)
                empty_.draw(scene);
            else
            {
                grid_.draw(scene);
                draw_selection_panel(scene);
            }
            draw_hints(list, {{ui::Button::cross, grid_.count() == 0       ? "Refresh"
                                                  : page_ == Page::library ? "View title"
                                                                           : "Manage save"},
                              {ui::Button::triangle, "Refresh"},
                              {ui::Button::square, source_usb_ ? "Console saves" : "USB saves"},
                              {ui::Button::l1, "Previous user"},
                              {ui::Button::r1, "Next user"},
                              {ui::Button::options, "Close"}});
            break;
        case Page::details:
            draw_save_hero(scene);
            details_.draw(scene);
            actions_.draw(scene);
            if (!export_sheet_.visible() && !account_sheet_.visible() && !import_dialog_.visible())
                draw_hints(list, {{ui::Button::cross, "Choose"},
                                  {ui::Button::circle, "Back"},
                                  {ui::Button::options, "Close"}});
            break;
        case Page::files:
            draw_mount_summary(scene);
            if (file_count_ == 0)
                empty_.draw(scene);
            else
                files_.draw(scene);
            draw_hints(list, {{ui::Button::triangle, "Refresh"},
                              {ui::Button::square, "Unmount"},
                              {ui::Button::circle, "Back"}});
            break;
        case Page::error:
            error_.draw(scene);
            draw_hints(list, {{ui::Button::cross, "Try again"}, {ui::Button::circle, "Close"}});
            break;
        case Page::loading:
        case Page::exporting:
        case Page::importing:
            break;
        }
        toasts_.draw(scene);
        if (export_sheet_.visible() || account_sheet_.visible() || import_dialog_.visible())
        {
            frame.glass = true;
            ui::Canvas overlay{frame.overlay, context_.fonts, frame.glass_texture, clock_};
            export_sheet_.draw(overlay);
            account_sheet_.draw(overlay);
            import_dialog_.draw(overlay);
            if (import_dialog_.visible())
                draw_hints(frame.overlay,
                           {{ui::Button::cross, "Choose"}, {ui::Button::circle, "Cancel"}});
            else if (account_sheet_.visible())
                draw_hints(frame.overlay, {{ui::Button::cross, "Choose account"},
                                           {ui::Button::l1, "Previous user"},
                                           {ui::Button::r1, "Next user"},
                                           {ui::Button::circle, "Back"}});
            else
                draw_hints(frame.overlay,
                           {{ui::Button::cross, export_drives_ ? "Choose drive" : "Choose export"},
                            {ui::Button::circle, "Back"}});
        }
    }

    std::span<const app::TourStep> tour() const override
    {
#if defined(__linux__)
        return kTour;
#else
        return kConsoleTour;
#endif
    }

    void shutdown() override
    {
        pending_artwork_.clear();
        worker_.stop();
    }

  private:
    const garlic::Save *selected_save() const
    {
        return selected_ >= 0 && static_cast<std::size_t>(selected_) < saves_.size()
                   ? &saves_[static_cast<std::size_t>(selected_)]
                   : nullptr;
    }

    const TitleEntry *selected_title() const
    {
        return selected_title_index_ >= 0 &&
                       static_cast<std::size_t>(selected_title_index_) < titles_.size()
                   ? &titles_[static_cast<std::size_t>(selected_title_index_)]
                   : nullptr;
    }

    const TitleEntry *current_title() const
    {
        const auto found = std::find_if(titles_.begin(), titles_.end(), [&](const TitleEntry &title)
                                        { return title.id == selected_title_; });
        return found == titles_.end() ? nullptr : &*found;
    }

    std::string current_title_name() const
    {
        if (const TitleEntry *title = current_title())
            return title->name;
        if (const garlic::Save *save = selected_save())
            return display_title(*save);
        return selected_title_;
    }

    void load_logo()
    {
#if defined(__linux__)
        const std::string path = std::string(HUI_SOURCE_DIR) + "/assets/branding/garlic-logo.png";
#else
        const std::string path = "/app0/assets/branding/garlic-logo.png";
#endif
        std::string bytes;
        std::string error;
        garlic::Image image;
        if (!hui::save::read_file(path, &bytes) || !garlic::decode_png(bytes, &image, &error))
        {
            hui::sys::log("[Garlic] branding unavailable: %s", error.c_str());
            return;
        }
        if (context_.texture_factory)
            logo_texture_ = context_.texture_factory(image.width, image.height, image.rgba.data());
    }

    static std::string artwork_key(std::string_view title_id, std::string_view save_name = {})
    {
        return save_name.empty() ? std::string(title_id)
                                 : std::string(title_id) + "/" + std::string(save_name);
    }

    std::uint32_t artwork_for(const garlic::Save &save) const
    {
        auto found = artwork_.find(artwork_key(save.title_id, save.save_name));
        if (found == artwork_.end())
            found = artwork_.find(artwork_key(save.title_id));
        return found == artwork_.end() ? 0 : found->second;
    }

    std::uint32_t artwork_for_title(std::string_view title_id) const
    {
        const auto found = artwork_.find(artwork_key(title_id));
        return found == artwork_.end() ? 0 : found->second;
    }

    float artwork_aspect_for(const garlic::Save &save) const
    {
        auto found = artwork_aspects_.find(artwork_key(save.title_id, save.save_name));
        if (found == artwork_aspects_.end())
            found = artwork_aspects_.find(artwork_key(save.title_id));
        return found == artwork_aspects_.end() ? 16.0f / 9.0f : found->second;
    }

    float artwork_aspect_for_title(std::string_view title_id) const
    {
        const auto found = artwork_aspects_.find(artwork_key(title_id));
        return found == artwork_aspects_.end() ? 16.0f / 9.0f : found->second;
    }

    void clear_artwork()
    {
        if (context_.texture_destructor)
            for (const auto &[key, texture] : artwork_)
                if (texture != 0)
                    context_.texture_destructor(texture);
        artwork_.clear();
        artwork_aspects_.clear();
    }

    void queue_artwork()
    {
        for (const garlic::Save &save : all_saves_)
        {
            if (artwork_.contains(artwork_key(save.title_id)))
                continue;
            const bool pending = std::any_of(pending_artwork_.begin(), pending_artwork_.end(),
                                             [&](const garlic::Save &candidate)
                                             { return candidate.title_id == save.title_id; });
            if (!pending)
            {
                garlic::Save metadata;
                metadata.title_id = save.title_id;
                metadata.title_name = save.title_name;
                metadata.type = save.type;
                pending_artwork_.push_back(std::move(metadata));
            }
        }
        request_next_artwork();
    }

    void request_next_artwork()
    {
        if (pending_artwork_.empty() || busy())
            return;
        const garlic::Save save = pending_artwork_.front();
        pending_artwork_.pop_front();
        start({garlic::JobType::load_icon, -1, save.title_id});
    }

    void apply_artwork(const garlic::Result &result)
    {
        if (result.image.rgba.empty())
        {
            request_next_artwork();
            return;
        }
        const std::string key =
            artwork_key(result.title_id, result.mounted_icon ? result.save_name : "");
        const float aspect =
            static_cast<float>(result.image.width) / static_cast<float>(result.image.height);
        const std::uint32_t texture =
            context_.texture_factory
                ? context_.texture_factory(result.image.width, result.image.height,
                                           result.image.rgba.data())
                : 0;
        const auto previous = artwork_.find(key);
        if (previous != artwork_.end() && previous->second != texture && previous->second != 0 &&
            context_.texture_destructor)
            context_.texture_destructor(previous->second);
        artwork_[key] = texture;
        artwork_aspects_[key] = aspect;
        if (page_ == Page::library)
        {
            for (int i = 0; i < static_cast<int>(titles_.size()); ++i)
            {
                if (titles_[static_cast<std::size_t>(i)].id == result.title_id &&
                    !result.mounted_icon)
                {
                    grid_.item(i).texture = texture;
                    grid_.item(i).image_aspect = aspect;
                }
            }
        }
        else if (page_ == Page::saves)
        {
            for (int i = 0; i < static_cast<int>(saves_.size()); ++i)
            {
                const garlic::Save &save = saves_[static_cast<std::size_t>(i)];
                if (save.title_id == result.title_id &&
                    (!result.mounted_icon || save.save_name == result.save_name))
                {
                    grid_.item(i).texture = texture;
                    grid_.item(i).image_aspect = aspect;
                }
            }
        }
        update_preview();
        request_next_artwork();
    }

    bool busy() const
    {
#if defined(__linux__)
        return fixture_delay_ > 0.0f || !results_.empty();
#else
        return worker_.busy();
#endif
    }

    void sync_actions()
    {
        if (actions_.count() != 2)
            return;
        const garlic::Save *save = selected_save();
        actions_.item(0).disabled = busy();
        actions_.item(1).label =
            save != nullptr && save->usb ? "Import USB save to PS5" : "Export to USB";
        actions_.item(1).disabled = busy() || save == nullptr;
    }

    void show_export_options(app::Feedback &feedback)
    {
        export_drives_ = false;
        export_options_.set_items({
            export_row("This save", "", -1, true),
            export_row("Decrypt to USB", "A readable folder; Garlic mounts and unmounts this save",
                       0),
            export_row("Every save in this title", "", -1, true),
            export_row("Encrypted images to USB",
                       "Raw, restorable images, with PS4 key files when present", 1),
            export_row("Decrypt to USB", "Readable folders for every save in this title", 2),
        });
        export_options_.set_focus(1);
        export_options_.set_active(true);
        export_options_.enter();
        Rect content = export_sheet_.content_rect();
        content.h -= 64;
        export_options_.set_bounds(content);
        export_sheet_.open(feedback);
    }

    void show_usb_drives(const std::vector<int> &drives)
    {
        std::vector<ui::ListItem> items;
        for (int drive : drives)
        {
            items.push_back(export_row("USB " + std::to_string(drive),
                                       "/mnt/usb" + std::to_string(drive) + "/PS5/SAVEDATA/GARLIC",
                                       drive));
        }
        if (drives.empty())
            items.push_back(export_row("No USB drive detected", "Connect a drive, then check again",
                                       -1, false, true));
        items.push_back(export_row("Check again", "Refresh the USB drive list", -2));
        export_options_.set_items(std::move(items));
        export_options_.set_focus(0);
        export_options_.enter();
    }

    void show_usb_error(const std::string &message)
    {
        export_options_.set_items({
            export_row("Could not check USB drives", message, -1, false, true),
            export_row("Try again", "Refresh the USB drive list", -2),
        });
        export_options_.set_focus(0);
        toasts_.push(ui::StatusKind::warning, "USB discovery failed", message, 6.0f);
    }

    void choose_export(int index)
    {
        const garlic::Save *save = selected_save();
        if (save == nullptr || index < 0 ||
            static_cast<std::size_t>(index) >= export_options_.items().size())
            return;
        const int tag = export_options_.items()[static_cast<std::size_t>(index)].tag;
        if (!export_drives_)
        {
            if (tag < 0 || tag > 2)
                return;
            pending_export_ = static_cast<garlic::UsbExportKind>(tag);
            export_drives_ = true;
            export_options_.set_items(
                {export_row("Checking USB drives", "Looking for connected storage", -1, true)});
            start({garlic::JobType::refresh_usb});
            return;
        }
        if (tag == -2)
        {
            start({garlic::JobType::refresh_usb});
            return;
        }
        if (tag >= 0 && tag <= 7)
        {
            start({garlic::JobType::usb_export, save->index, save->title_id, tag, pending_export_,
                   save->save_name});
        }
    }

    static std::string normalized_save_name(std::string name)
    {
        return name.starts_with("sdimg_") ? name.substr(6) : name;
    }

    void show_import_accounts(const garlic::Result &result, app::Feedback &feedback)
    {
        users_ = result.users;
        if (users_.empty())
        {
            toasts_.push(ui::StatusKind::danger, "No PS5 account found",
                         "Garlic could not find an account for this import.", 6.0f);
            return;
        }
        std::vector<ui::ListItem> items;
        for (int i = 0; i < static_cast<int>(users_.size()); ++i)
        {
            const garlic::User &user = users_[static_cast<std::size_t>(i)];
            items.push_back(
                export_row(user.name.empty() ? user.id : user.name, "Account ID: " + user.id, i));
        }
        account_users_.set_items(std::move(items));
        account_users_.set_focus(0);
        account_users_.set_active(true);
        account_users_.enter();
        Rect content = account_sheet_.content_rect();
        content.h -= 64;
        account_users_.set_bounds(content);
        account_sheet_.open(feedback);
    }

    void choose_import_account(int index, app::Feedback &feedback)
    {
        const garlic::Save *save = selected_save();
        if (save == nullptr || index < 0 || static_cast<std::size_t>(index) >= users_.size())
            return;
        const garlic::User &user = users_[static_cast<std::size_t>(index)];
        import_uid_ = user.id;
        import_user_name_ = user.name.empty() ? user.id : user.name;
        bool overwrite = false;
        for (const garlic::Save &candidate : all_saves_)
        {
            if (!candidate.usb && candidate.uid == import_uid_ &&
                candidate.title_id == save->title_id &&
                normalized_save_name(candidate.save_name) == normalized_save_name(save->save_name))
            {
                overwrite = true;
                break;
            }
        }
        hui::sys::log("[Garlic] import selection: %s / %s (%s) overwrite=%d",
                      save->title_id.c_str(), save->save_name.c_str(), display_title(*save).c_str(),
                      overwrite ? 1 : 0);
        std::string body = display_title(*save) + " / " + save->save_name +
                           "\nFor: " + import_user_name_ +
                           "\nGarlic will resign and import this save. ";
        body += overwrite ? "The existing system save will be replaced."
                          : "No matching save was found for this account.";
        account_sheet_.close(feedback);
        import_dialog_.open(
            {overwrite ? ui::StatusKind::danger : ui::StatusKind::warning,
             overwrite ? "Overwrite system save?" : "Import from USB?",
             body,
             {{"Cancel", ui::ButtonKind::secondary, false},
              {overwrite ? "Overwrite" : "Import", ui::ButtonKind::primary, overwrite}},
             0},
            feedback);
    }

    void confirm_import()
    {
        const garlic::Save *save = selected_save();
        if (save == nullptr || import_uid_.empty())
            return;
        import_dialog_.dismiss();
        selected_uid_ = import_uid_;
        start({garlic::JobType::import_save, save->index, save->title_id, -1,
               garlic::UsbExportKind::decrypted_save, save->save_name, import_uid_});
    }

    void set_page(Page page)
    {
        page_ = page;
        page_age_ = 0.0f;
        grid_.set_active(page == Page::library || page == Page::saves);
        actions_.set_active(page == Page::details);
        files_.set_active(page == Page::files);
        toasts_.style.anchor =
            page == Page::details ? ui::ToastAnchor::top_left : ui::ToastAnchor::top_right;
        const char *source = source_usb_ ? "USB titles" : "Console titles";
        switch (page)
        {
        case Page::loading:
            crumbs_.set_path({"Library", "Connecting"});
            break;
        case Page::library:
            crumbs_.set_path({"Library", source});
            rebuild_grid();
            update_empty();
            grid_.enter();
            preview_.enter();
            break;
        case Page::saves:
            crumbs_.set_path({"Library", source, current_title_name()});
            rebuild_grid();
            update_empty();
            grid_.enter();
            preview_.enter();
            break;
        case Page::details:
            if (const garlic::Save *save = selected_save())
                crumbs_.set_path({"Library", source, current_title_name(), save->save_name});
            details_.enter();
            break;
        case Page::files:
            if (const garlic::Save *save = selected_save())
                crumbs_.set_path(
                    {"Library", source, current_title_name(), save->save_name, "Files"});
            empty_.title = "No files found";
            empty_.body = "The mounted save has no files at this level.";
            empty_.action = "Refresh files";
            break;
        case Page::exporting:
            crumbs_.set_path({"Library", "USB export"});
            break;
        case Page::importing:
            crumbs_.set_path({"Library", "Import from USB"});
            break;
        case Page::error:
            crumbs_.set_path({"Library", "Connection problem"});
            break;
        }
        update_preview();
    }

    void start(const garlic::Job &job)
    {
        if (busy())
        {
            toasts_.push(ui::StatusKind::info, "One moment",
                         "Finishing the current Garlic request.");
            return;
        }
        if (job.type == garlic::JobType::startup)
        {
            set_page(Page::loading);
            loader_.set_stage(0, 0.0f);
        }
        else if (job.type == garlic::JobType::usb_export)
        {
            loader_.title = "Exporting to USB";
            loader_.subtitle = "Do not remove the drive. Garlic is copying your save data.";
            loader_.set_stages({{"Writing the export", 1}});
            loader_.set_progress(-1.0f);
            export_sheet_.dismiss();
            set_page(Page::exporting);
            loader_.show(feedback_placeholder_);
        }
        else if (job.type == garlic::JobType::import_save)
        {
            loader_.title = "Importing from USB";
            loader_.subtitle =
                "Do not remove the drive. Garlic is resigning and importing your save.";
            loader_.set_stages({{"Importing the save", 1}});
            loader_.set_progress(-1.0f);
            account_sheet_.dismiss();
            set_page(Page::importing);
            loader_.show(feedback_placeholder_);
        }
#if defined(__linux__)
        if (job.type != garlic::JobType::startup)
        {
            garlic::Result simulated;
            simulated.type = job.type;
            simulated.title_id = job.title_id;
            simulated.save_name = job.save_name;
            simulated.mounted_icon = job.mounted_icon;
            switch (job.type)
            {
            case garlic::JobType::refresh_saves:
                simulated.saves = all_saves_;
                simulated.message = "Save library refreshed";
                break;
            case garlic::JobType::mount:
            {
                const garlic::Save *save = selected_save();
                if (save != nullptr)
                {
                    simulated.mount.title_id = save->title_id;
                    simulated.mount.save_name = save->save_name;
                    simulated.mount.save_title = display_title(*save);
                    simulated.mount.mount = "/data/save_mnt/" + save->save_name;
                }
                simulated.mount.files = {{"sce_sys", true, 0},
                                         {"param.sfo", false, 1024},
                                         {"data.bin", false, 8192},
                                         { "checkpoint.dat",
                                           false,
                                           49152 }};
                simulated.files = simulated.mount.files;
                simulated.message = "Save mounted";
                break;
            }
            case garlic::JobType::refresh_files:
                simulated.files = {{"sce_sys", true, 0},
                                   {"param.sfo", false, 1024},
                                   {"data.bin", false, 8192},
                                   { "checkpoint.dat",
                                     false,
                                     49152 }};
                simulated.message = "File list refreshed";
                break;
            case garlic::JobType::unmount:
                simulated.message = "Save unmounted";
                break;
            case garlic::JobType::refresh_usb:
                simulated.usb_drives = {0};
                break;
            case garlic::JobType::usb_export:
            {
                simulated.export_result.ok = true;
                simulated.export_result.copied =
                    job.export_kind == garlic::UsbExportKind::decrypted_save ? -1 : 1;
                simulated.export_result.path =
                    "/mnt/usb" + std::to_string(job.usb) + "/PS5/SAVEDATA/GARLIC/" + job.title_id +
                    (job.export_kind == garlic::UsbExportKind::decrypted_save ? "/" + job.save_name
                                                                              : "");
                simulated.mount_released =
                    job.export_kind != garlic::UsbExportKind::encrypted_title;
                simulated.message = "USB export complete";
                simulated.saves = all_saves_;
                if (job.export_kind == garlic::UsbExportKind::decrypted_save)
                {
                    if (const garlic::Save *save = selected_save())
                    {
                        garlic::Save exported = *save;
                        exported.usb = true;
                        exported.uid.clear();
                        exported.path = simulated.export_result.path;
                        exported.dir = exported.title_id + "_" + exported.save_name;
                        exported.index = 0;
                        for (const garlic::Save &candidate : simulated.saves)
                            exported.index = std::max(exported.index, candidate.index + 1);
                        const bool duplicate =
                            std::any_of(simulated.saves.begin(), simulated.saves.end(),
                                        [&](const garlic::Save &candidate) {
                                            return candidate.usb && candidate.path == exported.path;
                                        });
                        if (!duplicate)
                            simulated.saves.push_back(std::move(exported));
                    }
                }
                fixture_delay_ = 0.65f;
                break;
            }
            case garlic::JobType::load_icon:
            {
                std::string bytes;
                std::string error;
                if (!hui::save::read_file(std::string(HUI_SOURCE_DIR) +
                                              "/tests/fixtures/garlic-landscape-4k.png",
                                          &bytes))
                    simulated.error = "The fixture game photo could not be read";
                else if (!garlic::decode_png(bytes, &simulated.image, &error))
                    simulated.error = error;
                break;
            }
            case garlic::JobType::prepare_import:
                simulated.users = {{"10000000", "Test User"}, { "20000000", "Other User" }};
                simulated.saves = all_saves_;
                break;
            case garlic::JobType::import_save:
            {
                garlic::Save imported;
                if (const garlic::Save *save = selected_save())
                    imported = *save;
                imported.usb = false;
                imported.uid = job.account_id;
                imported.path = "/system/" + imported.save_name;
                imported.dir = imported.save_name;
                simulated.import_result.title_id = imported.title_id;
                simulated.import_result.dir_name = imported.save_name;
                simulated.import_result.main_title = display_title(imported);
                simulated.saves = all_saves_;
                std::erase_if(simulated.saves,
                              [&](const garlic::Save &candidate)
                              {
                                  return !candidate.usb && candidate.uid == job.account_id &&
                                         candidate.title_id == imported.title_id &&
                                         normalized_save_name(candidate.save_name) ==
                                             normalized_save_name(imported.save_name);
                              });
                simulated.saves.push_back(std::move(imported));
                simulated.mount_released = true;
                simulated.message = "Save imported";
                fixture_delay_ = 0.65f;
                break;
            }
            case garlic::JobType::startup:
                break;
            }
            results_.push_back(std::move(simulated));
        }
#else
        if (!worker_.start(job))
        {
            error_.title = "Could not start Garlic";
            error_.body = "The background worker is unavailable. Close the app and try again.";
            set_page(Page::error);
        }
#endif
    }

    void poll_results(float dt, app::Feedback &feedback)
    {
        garlic::Result result;
        while (worker_.try_result(&result))
            apply(result, feedback);
#if !defined(__linux__)
        (void)dt;
#endif
#if defined(__linux__)
        if (fixture_delay_ > 0.0f)
        {
            fixture_delay_ = std::max(0.0f, fixture_delay_ - dt);
            return;
        }
        if (!results_.empty())
        {
            garlic::Result next = std::move(results_.front());
            results_.pop_front();
            apply(next, feedback);
        }
#endif
    }

    void release_mount()
    {
        mounted_by_app_ = false;
        daemon_mounted_ = false;
        mounted_ = {};
        update_files({});
        if (!details_.items().empty())
            details_.item(static_cast<int>(details_.items().size()) - 1).value = "No";
    }

    void apply(const garlic::Result &result, app::Feedback &feedback)
    {
        if (quitting_)
            return;
        if (result.mount_released)
        {
            release_mount();
            if (page_ == Page::files)
                set_page(Page::saves);
        }
        if (result.type == garlic::JobType::load_icon)
        {
            if (result.error.empty())
                apply_artwork(result);
            else
            {
                hui::sys::log("[Garlic] icon unavailable: %s", result.error.c_str());
                request_next_artwork();
            }
            return;
        }
        if (!result.error.empty())
        {
            if (result.type == garlic::JobType::refresh_usb)
            {
                show_usb_error(result.error);
                return;
            }
            if (result.type == garlic::JobType::usb_export)
            {
                set_page(Page::details);
                toasts_.push(ui::StatusKind::danger, "USB export failed", result.error, 6.0f);
                return;
            }
            if (result.type == garlic::JobType::prepare_import ||
                result.type == garlic::JobType::import_save)
            {
                set_page(Page::details);
                toasts_.push(ui::StatusKind::danger, "USB import failed", result.error, 6.0f);
                return;
            }
            error_.title = "Garlic request failed";
            error_.body = result.error;
            set_page(Page::error);
            toasts_.push(ui::StatusKind::danger, "Request failed", result.error, 6.0f);
            return;
        }
        switch (result.type)
        {
        case garlic::JobType::startup:
            daemon_mounted_ = result.daemon_mounted;
            users_ = result.users;
            selected_uid_ = result.account_id;
            if (users_.empty())
                selected_uid_.clear();
            else if (std::none_of(users_.begin(), users_.end(), [&](const garlic::User &user)
                                  { return user.id == selected_uid_; }))
                selected_uid_ = users_.front().id;
            update_saves(result.saves);
            loader_.set_stage(1, 1.0f);
            set_page(Page::library);
            toasts_.push(ui::StatusKind::success,
                         result.started_daemon ? "Garlic is ready" : "Connected to Garlic",
                         result.daemon_mounted
                             ? "An existing mount will be replaced when you open a save."
                             : "Your save library is ready.");
            break;
        case garlic::JobType::refresh_saves:
            update_saves(result.saves);
            toasts_.push(ui::StatusKind::success, result.message);
            break;
        case garlic::JobType::mount:
            mounted_ = result.mount;
            mounted_by_app_ = true;
            daemon_mounted_ = true;
            for (int i = 0; i < static_cast<int>(saves_.size()); ++i)
            {
                const garlic::Save &save = saves_[static_cast<std::size_t>(i)];
                if (save.title_id == mounted_.title_id && save.save_name == mounted_.save_name)
                {
                    selected_ = i;
                    grid_.set_focus(i);
                    update_preview();
                    update_palette();
                    break;
                }
            }
            update_files(result.files);
            set_page(Page::files);
            start({garlic::JobType::load_icon, -1, mounted_.title_id, -1,
                   garlic::UsbExportKind::decrypted_save, mounted_.save_name, "", true});
            break;
        case garlic::JobType::refresh_files:
            update_files(result.files);
            toasts_.push(ui::StatusKind::success, result.message);
            break;
        case garlic::JobType::unmount:
            release_mount();
            set_page(Page::saves);
            toasts_.push(ui::StatusKind::success, result.message);
            break;
        case garlic::JobType::refresh_usb:
            show_usb_drives(result.usb_drives);
            break;
        case garlic::JobType::usb_export:
        {
            if (!result.saves.empty())
                update_saves(result.saves);
            set_page(Page::saves);
            const garlic::UsbExport &exported = result.export_result;
            const bool failed = exported.failed > 0;
            std::string body = spaced_path(exported.path);
            if (exported.copied >= 0)
                body += "\n" + std::to_string(exported.copied) + " copied, " +
                        std::to_string(exported.failed) + " failed";
            toasts_.push(failed ? ui::StatusKind::warning : ui::StatusKind::success,
                         failed ? "USB export partially complete" : "USB export complete", body,
                         7.0f);
            start({garlic::JobType::refresh_saves});
            break;
        }
        case garlic::JobType::load_icon:
            break;
        case garlic::JobType::prepare_import:
            update_saves(result.saves);
            show_import_accounts(result, feedback);
            break;
        case garlic::JobType::import_save:
        {
            if (!result.saves.empty())
                update_saves(result.saves);
            set_page(Page::saves);
            const bool warning = !result.import_result.warning.empty();
            std::string body =
                (result.import_result.main_title.empty() ? result.import_result.title_id
                                                         : result.import_result.main_title) +
                " / " + result.import_result.dir_name;
            if (warning)
                body += "\n" + result.import_result.warning;
            toasts_.push(warning ? ui::StatusKind::warning : ui::StatusKind::success,
                         warning ? "Imported with warning" : "Save imported", body, 7.0f);
            break;
        }
        }
    }

    bool source_matches(const garlic::Save &save) const
    {
        return save.usb == source_usb_ &&
               (save.usb || selected_uid_.empty() || save.uid == selected_uid_);
    }

    void update_empty()
    {
        if (page_ == Page::library)
            empty_.title = source_usb_ ? "No USB titles found" : "No console titles found";
        else
            empty_.title =
                source_usb_ ? "No USB saves for this title" : "No console saves for this title";
        empty_.body = source_usb_ ? "Connect a USB drive with Garlic exports, then refresh. Square "
                                    "returns to console saves."
                                  : "There are no saves for this account. LB and RB switch "
                                    "accounts; Square shows USB saves.";
        empty_.action = "Refresh library";
    }

    void rebuild_grid()
    {
        const bool save_view =
            page_ == Page::saves || page_ == Page::details || page_ == Page::files;
        std::vector<ui::CardItem> cards;
        cards.reserve(save_view ? saves_.size() : titles_.size());
        if (save_view)
        {
            for (const garlic::Save &save : saves_)
            {
                ui::CardItem card;
                card.title = save.save_name;
                card.subtitle = save.usb ? "USB storage" : "Console storage";
                if (save.backup)
                    card.subtitle += "  ·  backup available";
                card.badge = save.usb ? "USB" : uppercase(save.type);
                card.top = save.type == "ps4" ? Color::rgb(0x59467f) : Color::rgb(0x28567b);
                card.bottom = save.type == "ps4" ? Color::rgb(0x282448) : Color::rgb(0x172d4c);
                card.accent = save.type == "ps4" ? Color::rgb(0xb8a1ed) : kBlue;
                card.texture = artwork_for(save);
                card.image_aspect = artwork_aspect_for(save);
                card.tag = save.index;
                cards.push_back(std::move(card));
            }
        }
        else
        {
            for (const TitleEntry &title : titles_)
            {
                const bool ps4 = title.type == "ps4";
                ui::CardItem card;
                card.title = title.name;
                card.subtitle = std::to_string(title.console_saves) + " console · " +
                                std::to_string(title.usb_saves) + " USB";
                card.badge = source_usb_ ? "USB" : uppercase(title.type);
                card.top = ps4 ? Color::rgb(0x59467f) : Color::rgb(0x28567b);
                card.bottom = ps4 ? Color::rgb(0x282448) : Color::rgb(0x172d4c);
                card.accent = ps4 ? Color::rgb(0xb8a1ed) : kBlue;
                card.texture = artwork_for_title(title.id);
                card.image_aspect = artwork_aspect_for_title(title.id);
                cards.push_back(std::move(card));
            }
        }
        grid_.set_items(std::move(cards));
        grid_.set_focus(save_view ? selected_ : selected_title_index_);
    }

    void update_saves(const std::vector<garlic::Save> &saves)
    {
        const int previous_index = selected_;
        std::string previous_name;
        if (const garlic::Save *save = selected_save())
            previous_name = save->save_name;
        all_saves_ = saves;
        titles_.clear();
        for (const garlic::Save &save : all_saves_)
        {
            if (!save.usb && !selected_uid_.empty() && save.uid != selected_uid_)
                continue;
            auto found = std::find_if(titles_.begin(), titles_.end(), [&](const TitleEntry &title)
                                      { return title.id == save.title_id; });
            if (found == titles_.end())
            {
                TitleEntry title;
                title.id = save.title_id;
                title.name = display_title(save);
                title.type = save.type;
                titles_.push_back(std::move(title));
                found = std::prev(titles_.end());
            }
            if (save.usb)
                ++found->usb_saves;
            else
                ++found->console_saves;
        }
        std::erase_if(titles_, [&](const TitleEntry &title)
                      { return source_usb_ ? title.usb_saves == 0 : title.console_saves == 0; });
        std::sort(titles_.begin(), titles_.end(),
                  [](const TitleEntry &left, const TitleEntry &right) {
                      return left.name != right.name ? left.name < right.name : left.id < right.id;
                  });
        if (titles_.empty())
        {
            selected_title_index_ = 0;
            if (page_ == Page::library)
                selected_title_.clear();
        }
        else
        {
            int index = 0;
            for (int i = 0; i < static_cast<int>(titles_.size()); ++i)
                if (titles_[static_cast<std::size_t>(i)].id == selected_title_)
                    index = i;
            selected_title_index_ = std::clamp(index, 0, static_cast<int>(titles_.size()) - 1);
            if (page_ == Page::library)
                selected_title_ = titles_[static_cast<std::size_t>(selected_title_index_)].id;
        }
        saves_.clear();
        for (const garlic::Save &save : all_saves_)
            if (save.title_id == selected_title_ && source_matches(save))
                saves_.push_back(save);
        std::sort(saves_.begin(), saves_.end(),
                  [](const garlic::Save &left, const garlic::Save &right)
                  { return left.save_name < right.save_name; });
        selected_ = 0;
        if (previous_index >= 0)
            for (int i = 0; i < static_cast<int>(saves_.size()); ++i)
                if (saves_[static_cast<std::size_t>(i)].save_name == previous_name)
                {
                    selected_ = i;
                    break;
                }
        rebuild_grid();
        update_empty();
        const char *source = source_usb_ ? "USB titles" : "Console titles";
        if (page_ == Page::library)
            crumbs_.set_path({"Library", source});
        else if (page_ == Page::saves)
            crumbs_.set_path({"Library", source, current_title_name()});
        update_preview();
        update_palette();
        queue_artwork();
    }

    void update_preview()
    {
        if (page_ == Page::library)
        {
            const TitleEntry *title = selected_title();
            if (title == nullptr)
            {
                preview_.set_items({});
                return;
            }
            const bool artwork = artwork_for_title(title->id) != 0;
            preview_.set_bounds({1184, artwork ? 514.0f : 458.0f, 616, artwork ? 228.0f : 284.0f});
            preview_.set_items({
                detail("Title ID", title->id),
                detail("Console saves", std::to_string(title->console_saves)),
                detail("USB saves", std::to_string(title->usb_saves)),
                detail("Open", "Cross to view saves"),
            });
            preview_.enter();
            return;
        }
        const garlic::Save *save = selected_save();
        if (save == nullptr)
        {
            preview_.set_items({});
            return;
        }
        const bool artwork = artwork_for(*save) != 0;
        preview_.set_bounds({1184, artwork ? 514.0f : 458.0f, 616, artwork ? 228.0f : 284.0f});
        preview_.set_items({
            detail("Title ID", save->title_id),
            detail("Format", uppercase(save->type)),
            detail("Location", save->usb ? "USB storage" : "System storage"),
            detail("Backup", save->backup ? "Available" : "Not yet"),
        });
        preview_.enter();
    }

    void update_palette()
    {
        const garlic::Save *save = selected_save();
        const TitleEntry *title = current_title();
        const bool ps4 =
            save != nullptr ? save->type == "ps4" : title != nullptr && title->type == "ps4";
        palette_[0].target(Color::rgb(0x0b1020));
        palette_[1].target(ps4 ? Color::rgb(0x352145) : Color::rgb(0x1a274b));
        palette_[2].target(ps4 ? Color::rgb(0x3e346c) : Color::rgb(0x1a506c));
        palette_[3].target(ps4 ? Color::rgb(0x705286) : Color::rgb(0x56528c));
    }

    std::string selected_user_name() const
    {
        if (selected_uid_.empty())
            return "All users";
        for (const garlic::User &user : users_)
            if (user.id == selected_uid_)
                return user.name.empty() ? user.id : user.name;
        return selected_uid_;
    }

    void cycle_user(int direction, app::Feedback &feedback)
    {
        if (users_.size() < 2)
            return;
        if (busy())
        {
            toasts_.push(ui::StatusKind::info, "One moment",
                         "Finishing the current Garlic request.");
            return;
        }
        int current = 0;
        for (int i = 0; i < static_cast<int>(users_.size()); ++i)
            if (users_[static_cast<std::size_t>(i)].id == selected_uid_)
                current = i;
        const int count = static_cast<int>(users_.size());
        current = (current + direction + count) % count;
        selected_uid_ = users_[static_cast<std::size_t>(current)].id;
        selected_ = -1;
        if (page_ == Page::library)
        {
            selected_title_index_ = 0;
            selected_title_.clear();
        }
        grid_.set_focus(0);
        feedback.play(audio::Cue::focus);
        start({garlic::JobType::refresh_saves});
    }

    void cycle_source(app::Feedback &feedback)
    {
        if (busy())
        {
            toasts_.push(ui::StatusKind::info, "One moment",
                         "Finishing the current Garlic request.");
            return;
        }
        source_usb_ = !source_usb_;
        selected_ = -1;
        if (page_ == Page::library)
        {
            selected_title_index_ = 0;
            selected_title_.clear();
        }
        feedback.play(audio::Cue::focus);
        start({garlic::JobType::refresh_saves});
    }

    void open_title(app::Feedback &feedback)
    {
        selected_title_index_ = grid_.focus();
        const TitleEntry *title = selected_title();
        if (title == nullptr)
            return;
        selected_title_ = title->id;
        selected_ = 0;
        update_saves(all_saves_);
        set_page(Page::saves);
        feedback.play(audio::Cue::select);
    }

    void cycle_account(int direction, app::Feedback &feedback)
    {
        if (users_.empty())
            return;
        const int count = static_cast<int>(users_.size());
        const int current = std::clamp(account_users_.focus(), 0, count - 1);
        account_users_.set_focus((current + direction + count) % count, false);
        feedback.play(audio::Cue::focus);
    }

    void open_details(app::Feedback &feedback)
    {
        garlic::Save *save = nullptr;
        if (grid_.focus() >= 0 && static_cast<std::size_t>(grid_.focus()) < saves_.size())
            save = &saves_[static_cast<std::size_t>(grid_.focus())];
        if (save == nullptr)
            return;
        selected_ = grid_.focus();
        details_.set_items({
            detail("Save information", "", true),
            detail("Save name", save->save_name),
            detail("Title ID", save->title_id),
            detail("Format", uppercase(save->type)),
            detail("Location", save->usb ? "USB storage" : "System storage"),
            detail("Backup copy", save->backup ? "Available" : "Not yet"),
            detail("Mounted here", mounted_by_app_ ? "Yes" : "No"),
        });
        actions_.set_items({
            {"Mount and browse files", busy(), false, 0},
            {save->usb ? "Import USB save to PS5" : "Export to USB", busy(), false, 1},
        });
        actions_.set_focus(0);
        set_page(Page::details);
        feedback.play(audio::Cue::select);
    }

    void run_action(int action, app::Feedback &feedback)
    {
        const garlic::Save *save = selected_save();
        if (save == nullptr)
            return;
        if (busy())
        {
            toasts_.push(ui::StatusKind::info, "One moment",
                         "Finishing the current Garlic request.");
            return;
        }
        if (action == 0)
            start({garlic::JobType::mount, save->index, {}});
        else if (action == 1 && save->usb)
            start({garlic::JobType::prepare_import});
        else if (action == 1)
            show_export_options(feedback);
    }

    void update_files(const std::vector<garlic::FileEntry> &files)
    {
        file_count_ = static_cast<int>(files.size());
        std::vector<ui::TableRow> rows;
        rows.reserve(files.size());
        int id = 0;
        for (const garlic::FileEntry &file : files)
        {
            ui::TableRow row;
            row.id = id++;
            row.cells = {{file.name},
                         {file.directory ? "Folder" : "File"},
                         {file.directory ? std::string("—") : format_size(file.size),
                          static_cast<double>(file.size)}};
            rows.push_back(std::move(row));
        }
        files_.set_rows(std::move(rows));
    }

    void begin_quit(app::Feedback &feedback)
    {
        if (quitting_)
            return;
        quitting_ = true;
        export_sheet_.dismiss();
        account_sheet_.dismiss();
        import_dialog_.dismiss();
        loader_.title = "Closing Garlic Saves";
        loader_.subtitle = "Finishing requests and unmounting saves opened here";
        loader_.set_stages({{"Closing safely", 1}});
        loader_.set_progress(-1.0f);
        loader_.show(feedback);
    }

    void draw_header(gfx::DrawList &list) const
    {
        garlic_mark(list, 124, 102, 56, kAccent, logo_texture_);
        ui::text(list, context_.fonts.display, "Garlic Saves", 178, 106, 50, kInk);
        ui::text(list, context_.fonts.regular, "Your library, ready to explore", 180, 144, 22,
                 kMuted);
        list.circle(1516, 102, 5, busy() ? kAccent : Color::rgb(0x75d5ae));
        ui::text(
            list, context_.fonts.semibold,
            context_.fonts.semibold.font->fit(busy() ? "WORKING" : selected_user_name(), 18, 272),
            kRight, 110, 18, kMuted, gfx::Align::right, 1.2f);
    }

    void draw_selection_panel(ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, context_.fonts, theme_);
        paint.panel({1160, 250, 664, 670});
        if (page_ == Page::library)
        {
            const TitleEntry *title = selected_title();
            if (title == nullptr)
                return;
            const std::uint32_t texture = artwork_for_title(title->id);
            const float aspect = artwork_aspect_for_title(title->id);
            if (texture != 0)
            {
                const Rect art{1196, 278, 192, 108};
                list.image(texture, art, ui::cover_uv(gfx::kFullUv, aspect, art),
                           Color::rgb(0xffffff), 16);
                ui::text(list, context_.fonts.semibold, "SELECTED TITLE", 1412, 310, 17, kAccent,
                         gfx::Align::left, 2.6f);
            }
            else
            {
                ui::text(list, context_.fonts.semibold, "SELECTED TITLE", 1196, 304, 17, kAccent,
                         gfx::Align::left, 2.6f);
            }
            ui::text(list, context_.fonts.display,
                     context_.fonts.display.font->fit(title->name, 38, 592), 1196,
                     texture != 0 ? 440 : 372, 38, kInk);
            ui::text(list, context_.fonts.regular,
                     std::to_string(title->console_saves) + " console · " +
                         std::to_string(title->usb_saves) + " USB",
                     1196, texture != 0 ? 484 : 416, 24, kMuted);
            preview_.draw(canvas);
            list.rounded_rect({1196, 794, 592, 1}, 0, kMuted.with_alpha(0.18f));
            ui::text(list, context_.fonts.regular, "Cross to view this title's saves", 1196, 844,
                     23, kInk);
            ui::text(list, context_.fonts.regular,
                     std::to_string(titles_.size()) +
                         (titles_.size() == 1 ? " title in this view" : " titles in this view"),
                     1196, 884, 21, kMuted);
            return;
        }
        const garlic::Save *save = selected_save();
        if (save == nullptr)
            return;
        const std::uint32_t texture = artwork_for(*save);
        if (texture != 0)
        {
            const Rect art{1196, 278, 192, 108};
            list.image(texture, art, ui::cover_uv(gfx::kFullUv, artwork_aspect_for(*save), art),
                       Color::rgb(0xffffff), 16);
            ui::text(list, context_.fonts.semibold, "SELECTED SAVE", 1412, 310, 17, kAccent,
                     gfx::Align::left, 2.6f);
        }
        else
        {
            ui::text(list, context_.fonts.semibold, "SELECTED SAVE", 1196, 304, 17, kAccent,
                     gfx::Align::left, 2.6f);
        }
        ui::text(list, context_.fonts.display,
                 context_.fonts.display.font->fit(display_title(*save), 38, 592), 1196,
                 texture != 0 ? 440 : 372, 38, kInk);
        ui::text(list, context_.fonts.regular,
                 context_.fonts.regular.font->fit(save->save_name, 24, 592), 1196,
                 texture != 0 ? 484 : 416, 24, kMuted);
        preview_.draw(canvas);
        list.rounded_rect({1196, 794, 592, 1}, 0, kMuted.with_alpha(0.18f));
        ui::text(list, context_.fonts.regular, "Cross to mount, export or import", 1196, 844, 23,
                 kInk);
        ui::text(list, context_.fonts.regular,
                 std::to_string(saves_.size()) +
                     (saves_.size() == 1 ? " save under this title" : " saves under this title"),
                 1196, 884, 21, kMuted);
    }

    void draw_save_hero(ui::Canvas &canvas) const
    {
        const garlic::Save *save = selected_save();
        if (save == nullptr)
            return;
        gfx::DrawList &list = canvas.list;
        const bool ps4 = save->type == "ps4";
        list.gradient_rect({kMargin, 250, 1008, 670}, 24,
                           ps4 ? Color::rgb(0x59467f) : Color::rgb(0x28567b),
                           ps4 ? Color::rgb(0x282448) : Color::rgb(0x172d4c));
        list.circle(600, 472, 166, Color::rgb(0xffffff, 0.045f));
        list.circle(600, 472, 134, Color::rgb(0xffffff, 0.04f));
        const std::uint32_t texture = artwork_for(*save);
        if (texture != 0)
        {
            const Rect art{136, 348, 576, 324};
            list.image(texture, art, ui::cover_uv(gfx::kFullUv, artwork_aspect_for(*save), art),
                       Color::rgb(0xffffff), 20);
        }
        else
        {
            garlic_mark(list, 600, 474, 208, kAccent.with_alpha(0.85f), logo_texture_);
        }
        ui::text(list, context_.fonts.semibold, "YOUR SAVE, AT A GLANCE", 132, 304, 17, kAccent,
                 gfx::Align::left, 2.4f);
        ui::text(list, context_.fonts.display,
                 context_.fonts.display.font->fit(display_title(*save), 46, 920), 132, 720, 46,
                 kInk);
        ui::text(list, context_.fonts.regular,
                 context_.fonts.regular.font->fit(save->save_name, 27, 920), 132, 772, 27, kMuted);
        ui::text(list, context_.fonts.mono, save->title_id, 132, 844, 23, kBlue);
        ui::text(list, context_.fonts.regular,
                 daemon_mounted_ ? "Garlic has a mounted save" : "Not mounted", 132, 884, 21,
                 kMuted);
    }

    void draw_mount_summary(ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, context_.fonts, theme_);
        paint.panel({kMargin, 242, 1728, 104});
        ui::text(list, context_.fonts.semibold,
                 context_.fonts.semibold.font->fit(mounted_.save_title, 26, 1000), 128, 286, 26,
                 kInk);
        ui::text(list, context_.fonts.regular,
                 context_.fonts.regular.font->fit(mounted_.mount, 20, 1110), 128, 320, 20, kMuted);
        list.circle(1748, 284, 6, Color::rgb(0x75d5ae));
        ui::text(list, context_.fonts.semibold, "MOUNTED", 1726, 292, 18, kAccent,
                 gfx::Align::right, 1.6f);
    }

    void draw_hints(gfx::DrawList &list, std::initializer_list<ui::Hint> hints) const
    {
        std::vector<ui::Hint> copy(hints);
        ui::draw_hints(list, context_.fonts, ui::GlyphStyle::dark(), copy.data(),
                       static_cast<int>(copy.size()), kRight, true);
    }

#if defined(__linux__)
    static std::vector<garlic::Save> fixtures()
    {
        return {
            {0, "PPSA10001", "autosave", "/save/a", "a", "Circuit Valley", "ps5", false, false,
             "10000000"},
            {1, "PPSA10002", "chapter-2", "/save/b", "b", "Starfarer", "ps5", true, false,
             "10000000"},
            {2, "CUSA10003", "slot-1", "/save/c", "c", "Moonlight Tactics", "ps4", false, true,
             "10000000"},
            {3, "PPSA10001", "manual-save", "/save/d", "d", "Circuit Valley", "ps5", false, false,
             "10000000"},
            {4, "CUSA10003", "slot-1", "/save/other", "slot-1", "Moonlight Tactics", "ps4", false,
             false, "20000000"},
        };
    }
#endif

    app::Context &context_;
    ui::Theme theme_;
    Page page_ = Page::loading;
    float clock_ = 0.0f;
    float page_age_ = 0.0f;
    int selected_ = 0;
    int selected_title_index_ = 0;
    int file_count_ = 0;
    bool mounted_by_app_ = false;
    bool daemon_mounted_ = false;
    bool quitting_ = false;
    bool source_usb_ = false;
    bool export_drives_ = false;
    float fixture_delay_ = 0.0f;
    garlic::UsbExportKind pending_export_ = garlic::UsbExportKind::decrypted_save;
    garlic::Mount mounted_{};
    std::string selected_title_;
    std::vector<TitleEntry> titles_;
    std::vector<garlic::Save> all_saves_;
    std::vector<garlic::Save> saves_;
    std::deque<garlic::Save> pending_artwork_;
    std::vector<garlic::User> users_;
    std::string selected_uid_;
    std::string import_uid_;
    std::string import_user_name_;
    std::unordered_map<std::string, std::uint32_t> artwork_;
    std::unordered_map<std::string, float> artwork_aspects_;
    std::uint32_t logo_texture_ = 0;
    garlic::Worker worker_;
    std::deque<garlic::Result> results_;
    app::Feedback feedback_placeholder_{};
    ui::LoadingScreen loader_;
    ui::GridView grid_;
    ui::DetailList preview_;
    ui::DetailList details_;
    ui::ButtonGroup actions_;
    ui::Table files_;
    ui::EmptyState empty_;
    ui::EmptyState error_;
    ui::Breadcrumb crumbs_;
    ui::ToastStack toasts_;
    ui::Sheet export_sheet_;
    ui::ListView export_options_;
    ui::Sheet account_sheet_;
    ui::ListView account_users_;
    ui::Dialog import_dialog_;
    ui::SpringColor palette_[4];
};

} // namespace

std::unique_ptr<app::Concept> make_garlic(app::Context &context)
{
    return std::make_unique<Garlic>(context);
}

} // namespace hui::concepts
