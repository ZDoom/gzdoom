//
// Project: GViZDoom
// File: GameState.hpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#pragma once

#include <type_traits>

#define GVIZDOOM_GAMESTATES(GAMESTATE) \
    GAMESTATE(bool,  LevelFinished)    \
    GAMESTATE(int,   NumberOfKills)    \
    GAMESTATE(int,   Health)           \
    GAMESTATE(int,   Armor)            \
    GAMESTATE(bool,  AttackDown)       \
    GAMESTATE(bool,  UseDown)          \
    GAMESTATE(int,   WeaponState)      \
    GAMESTATE(int,   ItemCount)        \
    GAMESTATE(int,   SecretCount)      \
    GAMESTATE(int,   DamageCount)      \
    GAMESTATE(int,   BonusCount)       \
    GAMESTATE(bool,  OnGround)         \
    GAMESTATE(float, X)                \
    GAMESTATE(float, Y)                \
    GAMESTATE(float, Z)                \
    GAMESTATE(float, Angle)            \

namespace gvizdoom {

// Generate the GameState enum
#define GVIZDOOM_GAMESTATE_ENUM(TYPE, NAME) NAME,

enum class GameState {
    GVIZDOOM_GAMESTATES(GVIZDOOM_GAMESTATE_ENUM)
    Undefined
};


// Generate the type counter for IDs
namespace detail {

template <GameState First, GameState... Rest>
struct TypeCounter {
    template<GameState T, GameState U> struct IsSame : std::false_type {};
    template<GameState T> struct IsSame<T, T> : std::true_type {};

    template <GameState T>
    static consteval int Id() {
        if constexpr(IsSame<T, First>::value)
            return 0;
        else
            return TypeCounter<Rest...>::template Id<T>() + 1;
    }
};

#define GVIZDOOM_GAMESTATE_NAME(TYPE, NAME) GameState::NAME,
using GameStateTypeCounter = TypeCounter<
GVIZDOOM_GAMESTATES(GVIZDOOM_GAMESTATE_NAME) GameState::Undefined>;

} // namespace detail


// Generate the info structs that contain mappings from GameState enum to type, name and id
#define GVIZDOOM_GAMESTATE_MAP(TYPE, NAME)                                                 \
template <>                                                                                \
struct GameStateInfo<GameState::NAME> {                                                    \
    using                   Type    = TYPE;                                                \
    static constexpr char   name[]  {#NAME};                                               \
    static constexpr int    id      {detail::GameStateTypeCounter::Id<GameState::NAME>()}; \
};                                                                                         \
template <>                                                                                \
struct GameStateIdInfo<detail::GameStateTypeCounter::Id<GameState::NAME>()> {              \
    using   Type    = TYPE;                                                                \
};

template <GameState T_GameState>
struct GameStateInfo {};
template <int T_GameStateId>
struct GameStateIdInfo {};

GVIZDOOM_GAMESTATES(GVIZDOOM_GAMESTATE_MAP)


// Implement deleter function for all the pointers in the container
namespace detail {
    template<GameState First, GameState... Rest>
    inline void deleteGameStateArray(void* gameStates[])
    {
        if (gameStates[0] != nullptr)
            delete static_cast<typename GameStateInfo<First>::Type*>(gameStates[0]);
        deleteGameStateArray<Rest...>(&gameStates[1]);
    }

    template<>
    inline void deleteGameStateArray<GameState::Undefined>(void* gameStates[]) {}
}


class GameStateContainer {
private:
    // Total number of game states
    static constexpr int    nGameStates {detail::GameStateTypeCounter::Id<GameState::Undefined>()};

    static constexpr void (*gameStateArrayDeleter)(void*[]) {
        &detail::deleteGameStateArray<GVIZDOOM_GAMESTATES(GVIZDOOM_GAMESTATE_NAME) GameState::Undefined>
    };

public:
    GameStateContainer() = default;
    GameStateContainer(const GameStateContainer&) = delete;
    GameStateContainer(GameStateContainer&&) = default;
    GameStateContainer& operator=(const GameStateContainer&) = delete;
    GameStateContainer& operator=(GameStateContainer&&) = default;
    ~GameStateContainer()
    {
        gameStateArrayDeleter(_gameStates);
    }

    template <GameState T_GameState>
    void set(const typename GameStateInfo<T_GameState>::Type& value)
    {
        using Info = GameStateInfo<T_GameState>;
        if (_gameStates[Info::id] == nullptr)
            _gameStates[Info::id] = new typename Info::Type();
        *static_cast<typename Info::Type*>(_gameStates[Info::id]) = value;
    }

    template <GameState T_GameState>
    const typename GameStateInfo<T_GameState>::Type& get()
    {
        using Info = GameStateInfo<T_GameState>;
        if (_gameStates[Info::id] == nullptr)
            _gameStates[Info::id] = new typename Info::Type();
        return *static_cast<typename Info::Type*>(_gameStates[Info::id]);
    }

private:
    void*   _gameStates[nGameStates]    = {};
};

} // namespace gvizdoom


// Undef macros
#undef GVIZDOOM_GAMESTATE_MAP
#undef GVIZDOOM_GAMESTATE_NAME
#undef GVIZDOOM_GAMESTATE_ENUM
#undef GVIZDOOM_GAMESTATES
