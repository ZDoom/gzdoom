//
// Project: GViZDoom
// File: TestUtils.hpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#pragma once


#include <cstdint>


inline uint32_t xorHash(const uint32_t* buffer, std::size_t size)
{
    if (buffer == nullptr || size == 0)
        return 0;

    // just a simple running XOR hash with rotation
    uint32_t hash = buffer[0];
    for (std::size_t i=1; i<size; ++i) {
        hash ^= std::rotl(buffer[i], (int)i);
    }
    return hash;
}
