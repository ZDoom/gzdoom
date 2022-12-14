//
// Project: GViZDoom
// File: TestGameConfig.cpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#include <gtest/gtest.h>
#include <gvizdoom/DoomGame.hpp>

#include "TestUtils.hpp"


#define BGRA_HASH imageHash( \
    reinterpret_cast<const uint32_t*>(doomGame.getPixelsBGRA()), \
    doomGame.getScreenWidth(), doomGame.getScreenHeight())


using namespace gvizdoom;


// Test different video parameters
TEST(TestGameConfig, VideoParams)
{
    auto& doomGame = DoomGame::instance();
    GameConfig gameConfig{0, nullptr, false, 640, 480, true, GameConfig::HUD_STATUSBAR, 2, 3, 1, 4};

    {
        // 640x480, truecolor
        gameConfig.videoWidth = 640;
        gameConfig.videoHeight = 480;
        gameConfig.videoTrueColor = true;
        doomGame.init(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(doomGame.getScreenWidth(), gameConfig.videoWidth);
        ASSERT_EQ(doomGame.getScreenHeight(), gameConfig.videoHeight);
        ASSERT_EQ(hash, 0x359bb26e);
    }
    {
        // 320x240, truecolor
        gameConfig.videoWidth = 320;
        gameConfig.videoHeight = 240;
        gameConfig.videoTrueColor = true;
        doomGame.init(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(doomGame.getScreenWidth(), gameConfig.videoWidth);
        ASSERT_EQ(doomGame.getScreenHeight(), gameConfig.videoHeight);
        ASSERT_EQ(hash, 0xa3610c0a);
    }
    {
        // 1280x960, truecolor
        gameConfig.videoWidth = 1280;
        gameConfig.videoHeight = 960;
        gameConfig.videoTrueColor = true;
        doomGame.restart(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(doomGame.getScreenWidth(), gameConfig.videoWidth);
        ASSERT_EQ(doomGame.getScreenHeight(), gameConfig.videoHeight);
        ASSERT_EQ(hash, 0x60e5d2e9);
    }
#if 0 // TODO for some reason the paletted rendering produces indeterministic results, subject to investigation
    {
        // 320x240, 256-color palette
        gameConfig.videoWidth = 320;
        gameConfig.videoHeight = 240;
        gameConfig.videoTrueColor = false;
        doomGame.restart(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(doomGame.getScreenWidth(), gameConfig.videoWidth);
        ASSERT_EQ(doomGame.getScreenHeight(), gameConfig.videoHeight);
        ASSERT_EQ(hash, 0xee967efb);
    }
    {
        // 1280x960, 256-color palette
        gameConfig.videoWidth = 1280;
        gameConfig.videoHeight = 960;
        gameConfig.videoTrueColor = false;
        doomGame.restart(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(doomGame.getScreenWidth(), gameConfig.videoWidth);
        ASSERT_EQ(doomGame.getScreenHeight(), gameConfig.videoHeight);
        ASSERT_EQ(hash, 0x50b10dff);
    }
#endif
}

#if 0
// Test different HUD parameters
TEST(TestGameConfig, HUDParams)
{
    auto& doomGame = DoomGame::instance();
    GameConfig gameConfig{0, nullptr, false, 640, 480, true, GameConfig::HUD_STATUSBAR, 2, 3, 1, 4};

    {
        // statusbar, scale 2
        gameConfig.hudType = GameConfig::HUD_STATUSBAR;
        gameConfig.hudScale = 2;
        doomGame.init(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(hash, 0x359bb26e);
    }
    {
        // statusbar, scale 1
        gameConfig.hudType = GameConfig::HUD_STATUSBAR;
        gameConfig.hudScale = 1;
        doomGame.init(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(hash, 0xeb5f5bf0);
    }
    {
        // floating hud, scale 2
        gameConfig.hudType = GameConfig::HUD_FLOATING;
        gameConfig.hudScale = 2;
        doomGame.restart(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(hash, 0x4739566a);
    }
    {
        // floating hud, scale 1
        gameConfig.hudType = GameConfig::HUD_FLOATING;
        gameConfig.hudScale = 1;
        doomGame.restart(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(hash, 0x127a8e8c);
    }
    {
        // alternative floating hud, scale 2
        gameConfig.hudType = GameConfig::HUD_ALTERNATIVE;
        gameConfig.hudScale = 2;
        doomGame.restart(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(hash, 0x837dad8c);
    }
    {
        // alternative floating hud, scale 1
        gameConfig.hudType = GameConfig::HUD_ALTERNATIVE;
        gameConfig.hudScale = 1;
        doomGame.restart(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = BGRA_HASH;
        ASSERT_EQ(hash, 0xc48f0080);
    }
}
#endif