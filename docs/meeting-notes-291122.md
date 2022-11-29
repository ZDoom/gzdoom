Meeting 29.11.2022
==================

Backlog
----------

- Save state [NOT STARTED]
    - Try several different actions from the state
- Benchmark the truecolor renderer [NOT STARTED]
    - Compare it with the original sw renderer
- Investigate action latency
    - Latency either in SDL event registration or on gvizdoom side
- Add more actions
    - Maybe also configurable keymap (`sdl::unordered_map` for example) for `SDLActionMapper`
- Get rid of all the SDL / OpenGL stuff inside the gvizdoom
    - HUD rendering an obstacle, investigation ongoing
- Save state
    - Try several different actions from the state
- Rewards
    - Implement as an external class to `DoomGame`
    - Return game state variables instead
- Game state
    - Figure out interface
- Config file
    - Currently loads `~/.config/gzdoom/gzdoom.ini`
        - Shared with GZDoom
    - Some of the config variables needed to be communicated via init eventually
        - Resolution for example
- Testing
    - Do a predetermined amount of actions (say, W, W, W, Mouse1 or something)
    - Make sure the end result is always the same
    - For example hash the screen pixel buffer and compare


Done 22.11.-29.11.
---------------------

- Decided to change to GZDoom 3.3.2 as basis version
  - New branch `develop_new`
  - Software rendering for hud not present or developed in more current versions anymore
  - OpenGL / alternative sw renderer removed
  - Project restructured, `gvizdoom` / `gvizdoom_client` sources migrated from old `develop` branch
  - Everything links together, but integration (doomloop slicing etc.) still TODO

Next things
-----------

- Integration
  - doomloop slicing and interaction with gvizdoom's `DoomGame` class
- SDL migration from gvizdoom

First stretch goal
---------------------

- Requirements for oblige map AI
    - Game states required:
        - Is level (episode) finished?
        - RGB pixels
    - Actions
        - tensor(`std::vector<float/double>`) <-> action conversion
            - how this is done?
            - model output considerations
        - don't force user to use all actions
        - don't force vector index -> action mapping ordering
            - configurability
        - Required for POC:
            - movement, use, mouselook(horizontal)
