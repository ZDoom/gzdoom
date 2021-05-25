/*
 ** st_console.cpp
 **
 **---------------------------------------------------------------------------
 ** Copyright 2015 Alexey Lysiuk
 ** Copyright 2021 Cacodemon345
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 **
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **---------------------------------------------------------------------------
 **
 */

#include "st_console.h"
#include "st_start.h"
#include "engineerrors.h"
#include "imgui_impl_sdl.h"
#include "imgui_colored_text.h"
#include "imgui_internal.h"
#include "i_interface.h"
#include "i_specialpaths.h"
#include "version.h"
#include <stdexcept>

static FConsoleWindow* m_console;
extern FStartupScreen* StartScreen;
extern int ProgressBarCurPos, ProgressBarMaxPos;
static TArray<FString> savedtexts;

EXTERN_CVAR (Bool, disableautoload)
EXTERN_CVAR (Bool, autoloadlights)
EXTERN_CVAR (Bool, autoloadbrightmaps)
EXTERN_CVAR (Bool, autoloadwidescreen)
EXTERN_CVAR (Int, vid_preferbackend)

void FConsoleWindow::CreateInstance()
{
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = NULL;
    ImGui::GetIO().LogFilename = NULL;
    try
    {
        m_console = new FConsoleWindow;
    }
    catch (const std::runtime_error& e)
    {
        fputs(e.what(),stderr);
        m_console = nullptr;
    }
}
bool FConsoleWindow::InstanceExists()
{
    return m_console != nullptr;
}
FConsoleWindow& FConsoleWindow::GetInstance()
{
    assert(m_console);
    return *m_console;
}

void FConsoleWindow::DeleteInstance()
{
    if (m_console != nullptr)
    {
        delete m_console, m_console = nullptr;
    }
}

FConsoleWindow::FConsoleWindow()
: io(ImGui::GetIO()), ProgBar(false), m_netinit(false), m_exitreq(false),
 m_error(false), m_iwadselect(false), m_maxscroll(0), m_errorframe(0)
{
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    FString windowtitle;
    windowtitle.Format("%s %s", GAMENAME, GetGitDescription());
    m_window = SDL_CreateWindow(windowtitle.GetChars(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 512, 384, SDL_WINDOW_OPENGL);
    if (m_window == nullptr)
    {
        throw std::runtime_error("Failed to create console window, falling back to terminal-only\n");
    }
    m_renderer = SDL_CreateRenderer(m_window, -1, strncmp(SDL_GetCurrentVideoDriver(), "KMSDRM", sizeof("KMSDRM")) == 0 ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED);
    ImGuiSDL::Initialize(m_renderer, 512, 384);
    ImGui_ImplSDL2_Init(m_window);
    m_texts.Append(savedtexts);
    savedtexts.Clear();
}

FConsoleWindow::~FConsoleWindow()
{
    if (m_window != nullptr)
    {
        savedtexts.Append(m_texts);
        m_texts.Clear();
        ImGuiSDL::Deinitialize();
        ImGui_ImplSDL2_Shutdown();
        SDL_DestroyRenderer(m_renderer);
        SDL_DestroyWindow(m_window);
        ImGui::DestroyContext();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

void FConsoleWindow::AddText(const char* message)
{
    AddText(FString(message));
}


void FConsoleWindow::AddText(FString message)
{
    // Not ideal, but makes sure we scroll all the way down.
    m_texts.Push(message);
    RunImguiRenderLoop();
    m_texts.Push(FString("\n"));
    RunImguiRenderLoop();
    m_texts.Pop();
    RunLoop();
}

void FConsoleWindow::SetProgressBar(bool visible)
{
    ProgBar = visible;
}

void FConsoleWindow::NetInit(const char* message, int playerCount)
{
    m_nettext = message;
    m_netinit = true;
    m_netMaxPos = playerCount;
    m_netCurPos = 0;
    SetProgressBar(false);
    AddText(TEXTCOLOR_GREEN "Press Q to abort network game synchronization.\n");
}

void FConsoleWindow::NetProgress(const int count)
{
    if (count == 0) m_netCurPos++;
    else m_netCurPos = count;
    if (m_netMaxPos == 0) m_netCurPos = m_netCurPos >= 100 ? 0 : m_netCurPos + 1;
    RunLoop();
}

void FConsoleWindow::NetDone()
{
    m_netinit = false;
    m_nettext = "";
}

void FConsoleWindow::ShowFatalError(const char *message)
{
    SetProgressBar(false);
    m_netinit = false;
    AddText(TEXTCOLOR_RED "Execution could not continue.\n");
    AddText("\n");
    AddText(message);
    m_error = true;
    while (m_error)
    {
        RunLoop();
    }
}

int FConsoleWindow::PickIWad(WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
    m_iwadselect = true;
    m_iwadparams.wads = wads;
    m_iwadparams.numwads = numwads;
    m_iwadparams.currentiwad = defaultiwad;
    m_iwadparams.curbackend = vid_preferbackend;
    m_iwadparams.lightsload = autoloadlights;
    m_iwadparams.brightmapsload = autoloadbrightmaps;
    m_iwadparams.widescreenload = autoloadwidescreen;
    m_iwadparams.noautoload = disableautoload;
    for (int i = 0; i < numwads; i++)
    {
        m_iwadparams.wadnames.Push(wads[i].Name.GetChars());
    }
    while (m_iwadselect)
    {
        RunLoop();
    }
    if (m_iwadparams.currentiwad != -1)
    {
        vid_preferbackend = m_iwadparams.curbackend;
        autoloadlights = m_iwadparams.lightsload;
        autoloadbrightmaps = m_iwadparams.brightmapsload;
        autoloadwidescreen = m_iwadparams.widescreenload;
        disableautoload = m_iwadparams.noautoload;
    }
    return m_iwadparams.currentiwad;
}

void FConsoleWindow::RunImguiRenderLoop()
{
    ImGui_ImplSDL2_NewFrame(m_window);
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(512, 370));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(70, 70, 70, 255));
    ImGui::Begin("Console", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::PushTextWrapPos();
    for (auto& curText : m_texts)
    {
        ImGui::TextAnsiColored(ImVec4(224, 224, 224, 255), curText.GetChars());
    }
    if (m_netinit)
    {
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(0,127,0,255));
        ImGui::ProgressBar(std::clamp(m_netMaxPos == 0 ? (double)m_netCurPos / 100. : (double)m_netCurPos / (double)m_netMaxPos, 0., 1.), ImVec2(-1, 0), m_nettext.GetChars());
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
    
    if (m_error)
    {
        if (ImGui::Button("Quit"))
        {
            m_error = false;
        }
        m_errorframe++;
    }
    else if (m_iwadselect)
    {
        if (!ImGui::IsPopupOpen("Game selection"))
        {
            ImGui::OpenPopup("Game selection");
        }
        ImGui::BeginPopupModal("Game selection", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text(GAMENAME " found more than one game\nSelect from the list below to\ndetermine which one to use:");
        ImGui::PushItemWidth(-1);
        if (ImGui::ListBoxHeader(""))
        {
            for (int i = 0; i < m_iwadparams.numwads; i++)
            {
                if (ImGui::Selectable(m_iwadparams.wads[i].Name.GetChars(), i == m_iwadparams.currentiwad))
                {
                    m_iwadparams.currentiwad = i;
                }
            }
            ImGui::ListBoxFooter();
        }
        ImGui::PopItemWidth();
        ImGui::Text("Renderer: ");
        ImGui::RadioButton("OpenGL", &m_iwadparams.curbackend, 0);
#ifdef HAVE_VULKAN
        ImGui::RadioButton("Vulkan", &m_iwadparams.curbackend, 1);
#endif
#ifdef HAVE_SOFTPOLY
        ImGui::RadioButton("Softpoly", &m_iwadparams.curbackend, 2);
#endif
        ImGui::Checkbox("Lights", &m_iwadparams.lightsload);
        ImGui::Checkbox("Brightmaps", &m_iwadparams.brightmapsload);
        ImGui::Checkbox("Widescreen", &m_iwadparams.widescreenload);
        ImGui::Checkbox("Disable autoload", &m_iwadparams.noautoload);
        if (ImGui::Button("OK"))
        {
            m_iwadselect = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_iwadparams.currentiwad = -1;
            m_iwadselect = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_errorframe <= 2) ImGui::SetScrollHereY(1.0f);
    ImGui::End();
    ImGui::Render();
}

bool FConsoleWindow::NetUserExitRequested()
{
    return m_netinit && m_exitreq;
}

void FConsoleWindow::RunLoop()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_Q && m_netinit))
        {
            m_exitreq = true;
        }
    }
    if (m_exitreq && !ProgBar && !m_netinit)
    {
        throw CExitEvent(0);
    }
    RunImguiRenderLoop();
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    ImGuiSDL::Render(ImGui::GetDrawData());
    if (ProgBar)
    {
        SDL_FRect rect = {0.f, 370.f, 512.f * ((float)ProgressBarCurPos / (float)ProgressBarMaxPos), 14.f};
        SDL_SetRenderDrawColor(m_renderer, 0, 127, 0, 255);
        SDL_RenderFillRectF(m_renderer, &rect);
    }
    SDL_RenderPresent(m_renderer);
}