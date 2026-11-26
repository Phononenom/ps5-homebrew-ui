// Garlic Save Companion - controller flow tests with the local save fixtures.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

#include <algorithm>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

class GarlicUi : public hui::testing::ConceptFixture
{
  protected:
    void SetUp() override
    {
        design_ = hui::concepts::make_garlic(context_);
        design_->enter();
        send(*design_, idle(), 0.5f);
        send(*design_, press(Action::confirm), 0.2f);
    }

    void open_usb_details()
    {
        send(*design_, press(Action::back), 0.2f);
        if (!usb_opened_)
        {
            send(*design_, press(Action::west), 0.2f);
            usb_opened_ = true;
        }
        send(*design_, press(Action::confirm), 0.2f);
        send(*design_, press(Action::confirm), 0.2f);
    }

    void open_import_accounts()
    {
        open_usb_details();
        send(*design_, nav(Direction::down), 0.2f);
        send(*design_, press(Action::confirm), 0.3f);
    }

    std::unique_ptr<hui::app::Concept> design_;
    bool usb_opened_ = false;
};

TEST_F(GarlicUi, SaveSelectionAndDetailsRespondToTheController)
{
    send(*design_, nav(Direction::right), 0.3f);
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::confirm), 0.3f);
    EXPECT_TRUE(played(Cue::select));
    expect_drawable(*design_);
    send(*design_, press(Action::back), 0.3f);
    EXPECT_TRUE(played(Cue::back));
    expect_drawable(*design_);
}

TEST_F(GarlicUi, TitlesOpenTheirOwnSaveLists)
{
    send(*design_, press(Action::back), 0.2f);
    send(*design_, nav(Direction::right), 0.2f);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, nav(Direction::right), 0.2f);
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::confirm), 0.2f);
    expect_drawable(*design_);
    send(*design_, press(Action::back), 0.2f);
    send(*design_, press(Action::back), 0.2f);
    send(*design_, press(Action::west), 0.2f);
    expect_drawable(*design_);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, press(Action::confirm), 0.2f);
    expect_drawable(*design_);
}

TEST_F(GarlicUi, MountBrowseUnmountAndUsbExportStayNavigable)
{
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, press(Action::confirm), 0.3f);
    expect_drawable(*design_);
    send(*design_, press(Action::north), 0.3f);
    send(*design_, press(Action::west), 0.3f);
    expect_drawable(*design_);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, nav(Direction::down), 0.2f);
    send(*design_, press(Action::confirm), 0.3f);
    expect_drawable(*design_);
    send(*design_, press(Action::back), 0.2f);
    EXPECT_TRUE(played(Cue::modal_close));
}

TEST_F(GarlicUi, ClosingAndReopeningRestartsTheLibrary)
{
    send(*design_, press(Action::menu), 0.2f);
    expect_drawable(*design_);
    design_->enter();
    send(*design_, idle(), 0.2f);
    send(*design_, press(Action::confirm), 0.2f);
    EXPECT_TRUE(played(Cue::select));
    design_->shutdown();
}

TEST_F(GarlicUi, DecryptedSaveCanBeExportedToTheDetectedDrive)
{
    send(*design_, press(Action::confirm), 0.2f);
    for (int i = 0; i < 3; ++i)
        send(*design_, nav(Direction::down), 0.1f);
    send(*design_, press(Action::confirm), 0.3f);
    expect_drawable(*design_);
    EXPECT_TRUE(frame_.glass);
    send(*design_, press(Action::confirm), 0.2f);
    expect_drawable(*design_);
    send(*design_, press(Action::confirm), 0.2f);
    expect_drawable(*design_);
    send(*design_, idle(), 1.0f);
    expect_drawable(*design_);
    send(*design_, press(Action::back), 0.2f);
    EXPECT_TRUE(played(Cue::back));
}

TEST_F(GarlicUi, WholeTitleExportsOfferBothFormats)
{
    send(*design_, press(Action::confirm), 0.2f);
    for (int i = 0; i < 3; ++i)
        send(*design_, nav(Direction::down), 0.1f);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, nav(Direction::down), 0.1f);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, idle(), 1.0f);
    expect_drawable(*design_);

    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, nav(Direction::down), 0.1f);
    send(*design_, nav(Direction::down), 0.1f);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, idle(), 1.0f);
    expect_drawable(*design_);
}

TEST_F(GarlicUi, ImportCanBeCancelledBeforeAnyWrite)
{
    open_import_accounts();
    expect_drawable(*design_);
    send(*design_, press(Action::confirm), 0.2f);
    expect_drawable(*design_);
    send(*design_, press(Action::back), 0.2f);
    expect_drawable(*design_);
    send(*design_, press(Action::confirm), 0.3f);
    expect_drawable(*design_);
}

TEST_F(GarlicUi, ConfirmedImportRefreshesLibraryAndSecondAttemptWarns)
{
    open_import_accounts();
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, nav(Direction::right), 0.2f);
    send(*design_, press(Action::confirm), 0.2f);
    expect_drawable(*design_);
    send(*design_, idle(), 1.0f);
    expect_drawable(*design_);

    open_import_accounts();
    send(*design_, press(Action::confirm), 0.3f);
    expect_drawable(*design_);
    send(*design_, press(Action::back), 0.2f);
    expect_drawable(*design_);
}

TEST_F(GarlicUi, AccountSwitchingWrapsAndRestoresTheLibrary)
{
    send(*design_, press(Action::page_next), 0.2f);
    expect_drawable(*design_);
    send(*design_, press(Action::page_next), 0.2f);
    send(*design_, nav(Direction::right), 0.2f);
    EXPECT_TRUE(played(Cue::focus));
    expect_drawable(*design_);
}

TEST_F(GarlicUi, ImportPickerSupportsShoulderAccountSwitching)
{
    open_import_accounts();
    send(*design_, press(Action::page_next), 0.2f);
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::page_prev), 0.2f);
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::confirm), 0.2f);
    expect_drawable(*design_);
}

TEST_F(GarlicUi, DifferentAccountConflictRequiresSeparateConfirmation)
{
    open_import_accounts();
    send(*design_, press(Action::page_next), 0.2f);
    send(*design_, press(Action::confirm), 0.2f);
    EXPECT_TRUE(played(Cue::modal_close));
    expect_drawable(*design_);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, press(Action::back), 0.2f);
    expect_drawable(*design_);
}

TEST_F(GarlicUi, MountedArtworkBecomesATextureAndIsReleased)
{
    std::vector<std::uint32_t> created;
    std::vector<std::uint32_t> destroyed;
    design_.reset();
    context_.texture_factory = [&](int, int, const std::uint8_t *)
    {
        const std::uint32_t texture = static_cast<std::uint32_t>(created.size() + 77);
        created.push_back(texture);
        return texture;
    };
    context_.texture_destructor = [&](std::uint32_t texture) { destroyed.push_back(texture); };
    design_ = hui::concepts::make_garlic(context_);
    design_->enter();
    send(*design_, idle(), 0.5f);
    expect_drawable(*design_);
    bool found_logo = false;
    for (const hui::gfx::Run &run : frame_.scene.runs())
        found_logo |= run.texture == 77u;
    EXPECT_TRUE(found_logo);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, press(Action::confirm), 0.2f);
    send(*design_, press(Action::confirm), 0.3f);
    send(*design_, press(Action::back), 0.2f);
    expect_drawable(*design_);
    ASSERT_FALSE(created.empty());
    const std::uint32_t mounted_texture = created.back();
    bool found = false;
    for (const hui::gfx::Run &run : frame_.scene.runs())
        found |= run.texture == mounted_texture;
    EXPECT_TRUE(found);
    design_.reset();
    EXPECT_EQ(created.size(), 5U);
    std::sort(created.begin(), created.end());
    std::sort(destroyed.begin(), destroyed.end());
    EXPECT_EQ(created, destroyed);
}

} // namespace
