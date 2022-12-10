//
// Project: GViZDoom
// File: main.cpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#include "gvizdoom_client/App.hpp"
#include "gvizdoom/gzdoom_main_wrapper.hpp"

//#include "CLI/CLI.hpp" // TODO

using namespace gvizdoom;


// Pipeline function for the event handling
void handleEvents(SDL_Event& event, App::Context& appContext)
{
    // Handle SDL events
    switch (event.type) {
        case SDL_QUIT:
            *appContext.quit = true;
            break;

        case SDL_KEYDOWN:
            // Skip events if imgui widgets are being modified
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    *appContext.quit = true;
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }

    appContext.actionMapper->addEvent(event);
}

// Pipeline function for rendering
void render(RenderContext& renderContext, App::Context& appContext)
{

}


int main(int argc, char** argv)
{
    bool interactive = true;
#if 0 // TODO
    // Parse CLI args
    CLI::App cliParser{"GVizDoom Client"};
    cliParser.add_option("-i,--interactive", interactive, "Interactive mode: human player, keyboard and mouse input");
    CLI11_PARSE(cliParser, argc, argv);
#endif

    // Setup app and render context
    App::Settings appSettings;
    appSettings.handleEvents = &handleEvents;
    appSettings.render = &render;

    RenderContext renderContext;

    GameConfig gameConfig{argc, argv, interactive, 640, 480, false};

    App app(appSettings, &renderContext, gameConfig);

    app.loop();

    return 0;
}