//
// Project: GViZDoom
// File: App.hpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#pragma once


#include <string>
#include <memory>
#include <SDL.h>

#include "gvizdoom/GameConfig.hpp"
#include "gvizdoom/DoomGame.hpp"

#include "gvizdoom_client/SDLActionMapper.hpp"


namespace gvizdoom {

struct RenderContext {};

class App {
public:
    // Settings for the application
    struct WindowSettings {
        std::string name;
        int64_t framerateLimit;

        explicit WindowSettings(
            const std::string& name = "GViZDoom",
            int64_t framerateLimit = 60) :
            name(name),
            framerateLimit(framerateLimit)
        {}
    };

    // Struct for App context to be passed to pipeline functions
    struct Context {
        WindowSettings*     windowSettings;
        SDL_Window*         window;
        bool*               quit;
        DoomGame*           doomGame;
        SDLActionMapper*    actionMapper;

        Context(App& app) :
            windowSettings  (&app._settings.window),
            window          (app._window),
            quit            (&app._quit),
            doomGame        (app._doomGame.get()),
            actionMapper    (&app._actionMapper)
        {}
    };

    struct Settings {
        WindowSettings window;

        // Pipeline function pointers for event handling and rendering
        void (*handleEvents)(SDL_Event& event, Context& appContext);
        void (*render)(RenderContext& context, Context& appContext);

        explicit Settings(
            const WindowSettings& window                            = WindowSettings(),
            void (*handleEvents)(SDL_Event&, Context& appContext)   = nullptr,
            void (*render)(RenderContext&, Context& appContext)     = nullptr
        ) :
            window          (window),
            handleEvents    (handleEvents),
            render          (render)
        {}
    };

    explicit App(
        const Settings& settings = Settings(),
        RenderContext* renderContext = nullptr,
        GameConfig gameConfig = GameConfig());

    ~App();

    void loop(void);
    void restart(GameConfig gameConfig);

    void setRenderContext(RenderContext* renderContext);

private:
    Settings                    _settings;
    SDL_Window*                 _window;
    SDL_Renderer*               _renderer;
    SDL_Texture*                _texture;
    bool                        _quit; // flag for quitting the application
    uint32_t                    _lastTicks;
    uint32_t                    _frameTicks;
    std::unique_ptr<DoomGame>   _doomGame;
    SDLActionMapper             _actionMapper;

    App::Context                _appContext;
    RenderContext*              _renderContext;

    void createSDLObjects(const GameConfig& gameConfig);
    void destroySDLObjects();
};

} // namespace gvizdoom
