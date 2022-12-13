//
// Project: GViZDoom
// File: TestGameState.cpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#include <gtest/gtest.h>
#include <gvizdoom/GameState.hpp>


using namespace gvizdoom;


TEST(TestGameState, PlaceHolder)
{
    GameStateContainer gameState;

    gameState.set<GameState::EpisodeFinished>(true);
    gameState.set<GameState::Health>(83);

    auto episodeFinished = gameState.get<GameState::EpisodeFinished>();
    ASSERT_EQ(episodeFinished, true);

    auto numberOfKills = gameState.get<GameState::NumberOfKills>();
    ASSERT_EQ(numberOfKills, 0);

    auto health = gameState.get<GameState::Health>();
    ASSERT_EQ(health, 83);
}
