//
// Project: GViZDoom
// File: Context.hpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#pragma once

#include <memory>


namespace gvizdoom {

class HeadlessFrameBuffer;


// struct for global game context
struct Context {
    std::unique_ptr<HeadlessFrameBuffer>    frameBuffer {nullptr};
};

} // namespace gvizdoom
