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


inline uint32_t imageHash(const uint32_t* buffer, int width, int height)
{
    if (buffer == nullptr || width == 0 || height == 0)
        return 0;

    uint32_t hash = 0x00000000;
    for (int j=0; j<height; ++j) {
        for (int i=0; i<width; ++i) {
            hash ^= std::rotl(buffer[width*j+i], i) * std::rotr(buffer[width*j+i], j);
        }
    }
    return hash;
}
