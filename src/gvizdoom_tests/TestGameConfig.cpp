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


using namespace gvizdoom;


// Test first BGRA frame on default game config
TEST(TestGameConfig, DefaultValues)
{
    auto& doomGame = DoomGame::instance();
    doomGame.init(GameConfig()); // init with default config
    doomGame.update(Action()); // pass a single default action

    // assert frame correctness via hashing
    auto hash = xorHash(
        reinterpret_cast<const uint32_t*>(doomGame.getPixelsBGRA()),
        doomGame.getScreenWidth()*doomGame.getScreenHeight());

    ASSERT_EQ(hash, 0x8ef0b3cb);
}

// Test different video parameters
TEST(TestGameConfig, VideoParams)
{
    auto& doomGame = DoomGame::instance();
    GameConfig gameConfig;

    {
        // 320x240, truecolor
        gameConfig.videoWidth = 320;
        gameConfig.videoHeight = 240;
        gameConfig.videoTrueColor = true;
        doomGame.init(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = xorHash(
            reinterpret_cast<const uint32_t*>(doomGame.getPixelsBGRA()),
            doomGame.getScreenWidth() * doomGame.getScreenHeight());

        ASSERT_EQ(doomGame.getScreenWidth(), gameConfig.videoWidth);
        ASSERT_EQ(doomGame.getScreenHeight(), gameConfig.videoHeight);
        ASSERT_EQ(hash, 0xd16d13cf);
    }
    {
        // 1280x960, truecolor
        gameConfig.videoWidth = 1280;
        gameConfig.videoHeight = 960;
        gameConfig.videoTrueColor = true;
        doomGame.restart(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = xorHash(
            reinterpret_cast<const uint32_t*>(doomGame.getPixelsBGRA()),
            doomGame.getScreenWidth() * doomGame.getScreenHeight());

        ASSERT_EQ(doomGame.getScreenWidth(), gameConfig.videoWidth);
        ASSERT_EQ(doomGame.getScreenHeight(), gameConfig.videoHeight);
        ASSERT_EQ(hash, 0x08aefaae);
    }
#if 0 // TODO for some reason the paletted rendering produces indeterministic results, subject to investigation
    {
        // 320x240, 256-color palette
        gameConfig.videoWidth = 320;
        gameConfig.videoHeight = 240;
        gameConfig.videoTrueColor = false;
        doomGame.restart(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = xorHash(
            reinterpret_cast<const uint32_t*>(doomGame.getPixelsBGRA()),
            doomGame.getScreenWidth() * doomGame.getScreenHeight());

        ASSERT_EQ(doomGame.getScreenWidth(), gameConfig.videoWidth);
        ASSERT_EQ(doomGame.getScreenHeight(), gameConfig.videoHeight);
        ASSERT_EQ(hash, 0x9450d1e1);
    }
    {
        // 1280x960, 256-color palette
        gameConfig.videoWidth = 1280;
        gameConfig.videoHeight = 960;
        gameConfig.videoTrueColor = false;
        doomGame.restart(gameConfig);
        doomGame.update(Action()); // pass a single default action

        auto hash = xorHash(
            reinterpret_cast<const uint32_t*>(doomGame.getPixelsBGRA()),
            doomGame.getScreenWidth() * doomGame.getScreenHeight());

        ASSERT_EQ(doomGame.getScreenWidth(), gameConfig.videoWidth);
        ASSERT_EQ(doomGame.getScreenHeight(), gameConfig.videoHeight);
        ASSERT_EQ(hash, 0x258e622a);
    }
#endif
}
