//
// Project: GViZDoom
// File: App.cpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#include "gvizdoom_client/App.hpp"
#include "gvizdoom/HeadlessFrameBuffer.hpp"
#include "DoomGame.hpp"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <thread>


using namespace gvizdoom;


App::App(
    const App::Settings& settings,
    RenderContext* renderContext,
    GameConfig gameConfig
) :
    _settings       (settings),
    _window         (nullptr),
    _renderer       (nullptr),
    _texture        (nullptr),
    _quit           (false),
    _lastTicks      (0),
    _frameTicks     (0),
    _appContext     (*this),
    _renderContext  (renderContext)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Error: Could not initialize SDL!\n");
        return;
    }
    createSDLObjects(gameConfig);


    DoomGame::instance().init(std::move(gameConfig));
}

App::~App()
{
    destroySDLObjects();
    SDL_Quit();
}

void App::loop(void)
{
    auto& doomGame = DoomGame::instance();

    if (doomGame.getStatus()) {
        // DoomGame uninitialized / in error state: cannot run loop
        printf("App::loop: Invalid DoomGame status: %d\n", doomGame.getStatus());
        return;
    }

    std::vector<Action> actions;
#if 1
    {
        for (size_t i = 0; i < 50; ++i)
            actions.emplace_back(Action::Key::ACTION_ATTACK, 0);

        for (size_t i = 0; i < 50; ++i)
            actions.emplace_back(static_cast<int>(Action::Key::ACTION_FORWARD | Action::Key::ACTION_ATTACK), 100);

        for (size_t i = 0; i < 50; ++i)
            actions.emplace_back(Action::Key::ACTION_FORWARD, 0);
    }
#endif
    size_t actionIndex = 0LLU;

    // Application main loop
    while (!_quit) {
#if 0
        // Event handling
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (_settings.handleEvents != nullptr)
                _settings.handleEvents(event, _appContext);
        }
#endif

#if 0
        if (actions.size() == 0 or actionIndex >= actions.size()) {
            printf("App: performed all actions\n\n");
            break;
        }
#endif
        // Update game
        // _quit = doomGame.update(actionIndex < actions.size() ? actions.at(actionIndex++) : Action());
        _quit = _quit || doomGame.update(_actionMapper);

        if (_quit) {
            printf("App: got quit\n");
            break;
        }

        auto screenHeight = doomGame.getScreenHeight();
        auto screenWidth = doomGame.getScreenWidth();

        // TODO opencv temp
        if (doomGame.getPixelsDepth() != nullptr) {
            // render depth
            cv::Mat depthMat(screenHeight, screenWidth, CV_32FC1, doomGame.getPixelsDepth());
            cv::imshow("depth", depthMat);
            cv::waitKey(1);
        }
        // TODO end of opencv temp

        // User-defined render
        if (_renderContext != nullptr && _settings.render != nullptr)
            _settings.render(*_renderContext, _appContext);

        // Render screen
        //SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255); // clearing is not actually required
        //SDL_RenderClear(_renderer);
        uint8_t* sdlPixels;
        int pitch;
        SDL_LockTexture(_texture, nullptr, reinterpret_cast<void**>(&sdlPixels), &pitch);
        assert(pitch == screenWidth*4);
        memcpy(sdlPixels, doomGame.getPixelsBGRA(), sizeof(uint8_t)*screenWidth*screenHeight*4);
        SDL_UnlockTexture(_texture);
        SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
        SDL_RenderPresent(_renderer);
#if 0
        SDL_Delay(1000/_settings.window.framerateLimit);

        uint32_t curTicks = SDL_GetTicks();
        _frameTicks = curTicks - _lastTicks;
        _lastTicks = curTicks;
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

void App::restart(GameConfig gameConfig)
{
    destroySDLObjects();
    DoomGame::instance().restart(gameConfig);
    createSDLObjects(gameConfig);
}

void App::setRenderContext(RenderContext* renderContext)
{
    _renderContext = renderContext;
}

void App::createSDLObjects(const GameConfig& gameConfig)
{
    _window = SDL_CreateWindow(
        _settings.window.name.c_str(),
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        gameConfig.videoWidth,
        gameConfig.videoHeight,
        SDL_WINDOW_SHOWN);
    if (_window == nullptr) {
        printf("Error: SDL Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }
    _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
    if (_renderer == nullptr) {
        printf("Error: SDL Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }
    _texture = SDL_CreateTexture(
        _renderer,
        SDL_PIXELFORMAT_BGRA32,
        SDL_TEXTUREACCESS_STREAMING,
        gameConfig.videoWidth,
        gameConfig.videoHeight);
    if (_texture == nullptr) {
        printf("Error: SDL Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }
}

void App::destroySDLObjects()
{
    // Destroy SDL objects and quit SDL subsystems
    if (_texture != nullptr)
        SDL_DestroyTexture(_texture);
    if (_renderer != nullptr)
        SDL_DestroyRenderer(_renderer);
    if (_window != nullptr)
        SDL_DestroyWindow(_window);
}
