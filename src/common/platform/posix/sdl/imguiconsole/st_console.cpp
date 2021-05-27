/*
 ** st_console.cpp
 **
 **---------------------------------------------------------------------------
 ** Copyright 2006-2007 Randy Heit
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
#include "startupinfo.h"
#include "engineerrors.h"
#include "imgui_impl_sdl.h"
#include "imgui_colored_text.h"
#include "imgui_internal.h"
#include "i_interface.h"
#include "i_specialpaths.h"
#include "version.h"
#include "filesystem.h"
#include "s_music.h"
#include <stdexcept>

// Hexen startup screen
#define ST_MAX_NOTCHES			32
#define ST_NOTCH_WIDTH			16
#define ST_NOTCH_HEIGHT			23
#define ST_PROGRESS_X			64			// Start of notches x screen pos.
#define ST_PROGRESS_Y			441			// Start of notches y screen pos.

#define ST_NETPROGRESS_X		288
#define ST_NETPROGRESS_Y		32
#define ST_NETNOTCH_WIDTH		4
#define ST_NETNOTCH_HEIGHT		16
#define ST_MAX_NETNOTCHES		8

static FConsoleWindow* m_console;
extern FStartupScreen* StartScreen;
extern int ProgressBarCurPos, ProgressBarMaxPos;
static TArray<FString> savedtexts;

EXTERN_CVAR (Bool, disableautoload)
EXTERN_CVAR (Bool, autoloadlights)
EXTERN_CVAR (Bool, autoloadbrightmaps)
EXTERN_CVAR (Bool, autoloadwidescreen)
EXTERN_CVAR (Int, vid_preferbackend)


//==========================================================================
//
// ST_Util_PlanarToChunky4
//
// Convert a 4-bpp planar image to chunky pixels.
//
//==========================================================================

void ST_Util_PlanarToChunky4(uint8_t* dest, const uint8_t* src, int width, int height)
{
	int y, x;
	const uint8_t* src1, * src2, * src3, * src4;
	size_t plane_size = width / 8 * height;

	src1 = src;
	src2 = src1 + plane_size;
	src3 = src2 + plane_size;
	src4 = src3 + plane_size;

	for (y = height; y > 0; --y)
	{
		for (x = width; x > 0; x -= 8)
		{
			// Pixels 0 and 1
			dest[0] = (*src4 & 0x80) | ((*src3 & 0x80) >> 1) | ((*src2 & 0x80) >> 2) | ((*src1 & 0x80) >> 3) |
				((*src4 & 0x40) >> 3) | ((*src3 & 0x40) >> 4) | ((*src2 & 0x40) >> 5) | ((*src1 & 0x40) >> 6);
			// Pixels 2 and 3
			dest[1] = ((*src4 & 0x20) << 2) | ((*src3 & 0x20) << 1) | ((*src2 & 0x20)) | ((*src1 & 0x20) >> 1) |
				((*src4 & 0x10) >> 1) | ((*src3 & 0x10) >> 2) | ((*src2 & 0x10) >> 3) | ((*src1 & 0x10) >> 4);
			// Pixels 4 and 5
			dest[2] = ((*src4 & 0x08) << 4) | ((*src3 & 0x08) << 3) | ((*src2 & 0x08) << 2) | ((*src1 & 0x08) << 1) |
				((*src4 & 0x04) << 1) | ((*src3 & 0x04)) | ((*src2 & 0x04) >> 1) | ((*src1 & 0x04) >> 2);
			// Pixels 6 and 7
			dest[3] = ((*src4 & 0x02) << 6) | ((*src3 & 0x02) << 5) | ((*src2 & 0x02) << 4) | ((*src1 & 0x02) << 3) |
				((*src4 & 0x01) << 3) | ((*src3 & 0x01) << 2) | ((*src2 & 0x01) << 1) | ((*src1 & 0x01));
			dest += 4;
			src1 += 1;
			src2 += 1;
			src3 += 1;
			src4 += 1;
		}
	}
}

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
 m_error(false), m_renderiwadtitle(false), m_graphicalstartscreen(false),
 m_consolewidth(512), m_consoleheight(384), m_iwadselect(false), m_maxscroll(0), m_errorframe(0)
{
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    FString windowtitle;
    windowtitle.Format("%s %s", GAMENAME, GetGitDescription());
    m_window = SDL_CreateWindow(windowtitle.GetChars(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 512, 384, SDL_WINDOW_OPENGL);
    if (m_window == nullptr)
    {
        throw std::runtime_error("Failed to create console window, falling back to terminal-only\n");
    }
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    SDL_GetRendererOutputSize(m_renderer, &m_consolewidth, &m_consoleheight);
    ImGuiSDL::Initialize(m_renderer, m_consolewidth, m_consoleheight);
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
        SetStartupType(StartupType::StartupTypeNormal);
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

void FConsoleWindow::InitGraphicalMode()
{
    m_graphicalstartscreen = true;
}

FConsoleWindow::StartupType FConsoleWindow::GetStartupType()
{
    return m_startuptype;
}

// Convert 4-bit pixels to 8-bit ones.
void ST_Util_Chunky4ToChunky8(uint8_t* dest, const uint8_t* src, int width, int height)
{
    for (int i = 0; i < (width * height) / 2; i++)
    {
        dest[i * 2 + 1] = src[i] & 0xF;
        dest[i * 2] = (src[i] >> 4) & 0xF;
    }
}

void FConsoleWindow::SetStartupType(StartupType type)
{
    switch (type)
    {
        case StartupType::StartupTypeNormal:
        {
            SDL_SetWindowMinimumSize(m_window, m_consolewidth, m_consoleheight);
            SDL_SetWindowSize(m_window, m_consolewidth, m_consoleheight);
            SDL_RenderSetLogicalSize(m_renderer, m_consolewidth, m_consoleheight);

            if (m_hexenpic) SDL_DestroyTexture(m_hexenpic);
            if (m_hexennetnotchpic) SDL_DestroyTexture(m_hexennetnotchpic);
            if (m_hexennotchpic) SDL_DestroyTexture(m_hexennotchpic);
        }
        break;

        case StartupType::StartupTypeHexen:
        {
            if (m_consolewidth != 640 && m_consoleheight != 480)
            {
                SDL_SetWindowSize(m_window, 640, 480);
                SDL_SetWindowMinimumSize(m_window, 640, 480);
                SDL_RenderSetLogicalSize(m_renderer, 640, 480);
            }

            auto hexenpixels = SDL_CreateRGBSurface(0, 640, 480, 8, 0, 0, 0, 0);
            auto hexennotchpixels = SDL_CreateRGBSurface(0, ST_NOTCH_WIDTH, ST_NOTCH_HEIGHT, 8, 0, 0, 0, 0);
            auto hexennetnotchpixels = SDL_CreateRGBSurface(0, ST_NETNOTCH_WIDTH, ST_NETNOTCH_HEIGHT, 8, 0, 0, 0, 0);
            
            if (!hexenpixels || !hexennotchpixels || !hexennetnotchpixels)
            {
                SDL_FreeSurface(hexenpixels);
                SDL_FreeSurface(hexennotchpixels);
                SDL_FreeSurface(hexennetnotchpixels);
                return;
            }

          	int startup_lump = fileSystem.CheckNumForName("STARTUP");
	        int netnotch_lump = fileSystem.CheckNumForName("NETNOTCH");
	        int notch_lump = fileSystem.CheckNumForName("NOTCH");
            if (startup_lump < 0 || fileSystem.FileLength(startup_lump) != 153648 ||
    		    netnotch_lump < 0 || fileSystem.FileLength(netnotch_lump) != ST_NETNOTCH_WIDTH / 2 * ST_NETNOTCH_HEIGHT ||
	    	    notch_lump < 0 || fileSystem.FileLength(notch_lump) != ST_NOTCH_WIDTH / 2 * ST_NOTCH_HEIGHT)
            {
                return;
            }
            uint8_t* startupcontents = new uint8_t[153648];
            uint8_t* startup4bitchunky = new uint8_t[(640 * 480) / 2];
            uint8_t* notch4bit = new uint8_t[ST_NOTCH_WIDTH / 2 * ST_NOTCH_HEIGHT];
            uint8_t* netnotch4bit = new uint8_t[ST_NETNOTCH_WIDTH / 2 * ST_NETNOTCH_HEIGHT];
            fileSystem.ReadFile(startup_lump, startupcontents);
            fileSystem.ReadFile(notch_lump, notch4bit);
            fileSystem.ReadFile(netnotch_lump, netnotch4bit);

            // Convert to 4-bit chunky pixels.
            ST_Util_PlanarToChunky4((uint8_t*)startup4bitchunky, startupcontents + 48, 640, 480);
            // Convert the graphics to 8-bit pixels.
            ST_Util_Chunky4ToChunky8((uint8_t*)hexenpixels->pixels, startup4bitchunky, 640, 480);
            ST_Util_Chunky4ToChunky8((uint8_t*)hexennotchpixels->pixels, notch4bit, ST_NOTCH_WIDTH, ST_NOTCH_HEIGHT);
            ST_Util_Chunky4ToChunky8((uint8_t*)hexennetnotchpixels->pixels, netnotch4bit, ST_NETNOTCH_WIDTH, ST_NETNOTCH_HEIGHT);
            delete[] notch4bit;
            delete[] netnotch4bit;
            delete[] startup4bitchunky;
            // Load the palette.
            auto palette4bit = SDL_AllocPalette(256);
            if (!palette4bit) return;
            auto colorpalette = new SDL_Color[256];
            for (int i = 0; i < 16; i++)
            {
                colorpalette[i].r = startupcontents[i * 3] << 2;
                colorpalette[i].g = startupcontents[i * 3 + 1] << 2;
                colorpalette[i].b = startupcontents[i * 3 + 2] << 2;
                colorpalette[i].a = 255;
            }
            for (int i = 16; i < 256; i++)
            {
                colorpalette[i].r = 0;
                colorpalette[i].g = 0;
                colorpalette[i].b = 0;
                colorpalette[i].a = 255;
            }
            SDL_SetPaletteColors(palette4bit, colorpalette, 0, 256);
            SDL_SetSurfacePalette(hexenpixels, palette4bit);
            SDL_SetSurfacePalette(hexennotchpixels, palette4bit);
            SDL_SetSurfacePalette(hexennetnotchpixels, palette4bit);
            delete[] startupcontents;
            delete[] colorpalette;

            m_hexenpic = SDL_CreateTextureFromSurface(m_renderer, hexenpixels);
            m_hexennotchpic = SDL_CreateTextureFromSurface(m_renderer, hexennotchpixels);
            m_hexennetnotchpic = SDL_CreateTextureFromSurface(m_renderer, hexennetnotchpixels);
            if (!m_hexennetnotchpic) return;
            if (!m_hexenpic) return;
            if (!m_hexennotchpic) return;
            SDL_FreeSurface(hexenpixels);
            SDL_FreeSurface(hexennotchpixels);
            SDL_FreeSurface(hexennetnotchpixels);
            if (!batchrun)
            {
                if (GameStartupInfo.Song.IsNotEmpty())
                {
                    S_ChangeMusic(GameStartupInfo.Song.GetChars(), true, true);
                }
                else
                {
                    S_ChangeMusic("orb", true, true);
                }
            }
        }
        break;
    }
    m_startuptype = type;
}

void FConsoleWindow::DeinitGraphicalMode()
{
    m_graphicalstartscreen = false;
}

void FConsoleWindow::ShowFatalError(const char *message)
{
    SetProgressBar(false);
    m_netinit = false;
    AddText(TEXTCOLOR_RED "Execution could not continue.\n");
    AddText("\n");
    FString fmtstring;
    fmtstring.Format(TEXTCOLOR_YELLOW "%s", message);
    AddText(fmtstring);
    m_error = true;
    m_renderiwadtitle = false;
    m_graphicalstartscreen = false;
    SetStartupType(StartupType::StartupTypeNormal);
    while (m_error)
    {
        RunLoop();
    }
}

void FConsoleWindow::SetTitleText()
{
    if (GameStartupInfo.FgColor == GameStartupInfo.BkColor)
	{
		GameStartupInfo.FgColor = ~GameStartupInfo.FgColor;
	}
    m_renderiwadtitle = true;
    m_iwadtitle = GameStartupInfo.Name;
    auto iwadtitletextcol = PalEntry(GameStartupInfo.FgColor);
    auto iwadtitlebgcol = PalEntry(GameStartupInfo.BkColor);
    m_iwadtitletextcol = IM_COL32(iwadtitletextcol.r, iwadtitletextcol.g, iwadtitletextcol.b, 255);
    m_iwadtitlebgcol = IM_COL32(iwadtitlebgcol.r, iwadtitlebgcol.g, iwadtitlebgcol.b, 255);
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
    if (!m_graphicalstartscreen)
    {
        ImGui::SetNextWindowPos(ImVec2(0, m_renderiwadtitle ? 32.f : 0));
        ImGui::SetNextWindowSize(ImVec2(m_consolewidth, m_consoleheight - (ProgBar ? 14.f : 0.f)));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(70, 70, 70, 255));
        ImGui::Begin("Console", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::PushTextWrapPos();
        for (unsigned int i = 0; i < m_texts.Size(); i++)
        {
            auto& curText = m_texts[i];
            ImGui::TextAnsiColored(ImVec4(224, 224, 224, 255), "%s", curText.GetChars());
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
        if (m_renderiwadtitle)
        {
            ImGui::SetNextWindowSize(ImVec2(m_consolewidth, 32.f));
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            if (ImGui::Begin("IWADInfo", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
            {
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, m_iwadtitlebgcol);
                ImGui::PushStyleColor(ImGuiCol_Text, m_iwadtitletextcol);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
                ImGui::ProgressBar(1.0f, ImVec2(-1,0), m_iwadtitle.GetChars());
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
            }
            ImGui::End();
        }
    }

    ImGui::Render();
}

bool FConsoleWindow::NetUserExitRequested()
{
    return m_netinit && m_exitreq;
}

void FConsoleWindow::RunHexenSubLoop()
{
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
    SDL_RenderCopy(m_renderer, m_hexenpic, NULL, NULL);
    static int notch_pos = 0;
    if (notch_pos != (ProgressBarCurPos * ST_MAX_NOTCHES) / ProgressBarMaxPos)
    {
        notch_pos = (ProgressBarCurPos * ST_MAX_NOTCHES) / ProgressBarMaxPos;
        if (sysCallbacks.PlayStartupSound) sysCallbacks.PlayStartupSound("StartupTick");
    }
    for (int i = 0; i < notch_pos; i++)
    {
        SDL_Rect dstrect;
        dstrect.x = ST_PROGRESS_X + ST_NOTCH_WIDTH * i;
        dstrect.y = ST_PROGRESS_Y;
        dstrect.w = ST_NOTCH_WIDTH;
        dstrect.h = ST_NOTCH_HEIGHT;
        SDL_Rect srcrect = (SDL_Rect){0, 0, ST_NOTCH_WIDTH, ST_NOTCH_HEIGHT};
        SDL_RenderCopy(m_renderer, m_hexennotchpic, &srcrect, &dstrect);
    }
    if (m_netinit)
    {
        static int netnotch_pos = 0;
        if (netnotch_pos != (m_netCurPos * ST_MAX_NETNOTCHES) / m_netMaxPos)
        {
            netnotch_pos = (m_netCurPos * ST_MAX_NETNOTCHES) / m_netMaxPos;
            if (sysCallbacks.PlayStartupSound) sysCallbacks.PlayStartupSound("misc/netnotch");
        }
        for (int i = 0; i < netnotch_pos; i++)
        {
            SDL_Rect dstrect;
            dstrect.x = ST_NETPROGRESS_X + ST_NETNOTCH_WIDTH * i;
            dstrect.y = ST_NETPROGRESS_Y;
            dstrect.w = ST_NETNOTCH_WIDTH;
            dstrect.h = ST_NETNOTCH_HEIGHT;
            SDL_Rect srcrect = (SDL_Rect){0, 0, ST_NETNOTCH_WIDTH, ST_NETNOTCH_HEIGHT};
            SDL_RenderCopy(m_renderer, m_hexennetnotchpic, &srcrect, &dstrect);
        }
    }
    SDL_RenderPresent(m_renderer);
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
        if (e.type == SDL_WINDOWEVENT_RESIZED)
        {
            m_consolewidth = e.window.data1;
            m_consoleheight = e.window.data2;
        }
    }
    if (m_exitreq && !ProgBar && !m_netinit)
    {
        throw CExitEvent(0);
    }
    if (m_startuptype != StartupType::StartupTypeNormal)
    {
        switch(m_startuptype)
        {
        case StartupType::StartupTypeHexen:
            RunHexenSubLoop();
            break;
        }
        return;
    }
    RunImguiRenderLoop();
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    ImGuiSDL::Render(ImGui::GetDrawData());
    if (ProgBar)
    {
        SDL_FRect rect = {0.f, m_consoleheight - 14.f, m_consolewidth * ((float)ProgressBarCurPos / (float)ProgressBarMaxPos), 14.f};
        SDL_SetRenderDrawColor(m_renderer, 0, 127, 0, 255);
        SDL_RenderFillRectF(m_renderer, &rect);
    }
    SDL_RenderPresent(m_renderer);
}