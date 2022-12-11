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
