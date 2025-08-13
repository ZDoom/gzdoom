#include <iostream>
#include <zwidget/core/widget.h>
#include <zwidget/core/resourcedata.h>
#include <zwidget/core/image.h>
#include <zwidget/core/theme.h>
#include <zwidget/window/window.h>
#include <zwidget/widgets/dropdown/dropdown.h>
#include <zwidget/widgets/textedit/textedit.h>
#include <zwidget/widgets/mainwindow/mainwindow.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/imagebox/imagebox.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>
#include "picopng.h"

static std::vector<uint8_t> ReadAllBytes(const std::string& filename);

class LauncherWindow : public Widget
{
public:
	LauncherWindow() : Widget(nullptr, WidgetType::Window)
	{
		Logo = new ImageBox(this);
		WelcomeLabel = new TextLabel(this);
		VersionLabel = new TextLabel(this);
		SelectLabel = new TextLabel(this);
		GeneralLabel = new TextLabel(this);
		ExtrasLabel = new TextLabel(this);
		FullscreenCheckbox = new CheckboxLabel(this);
		DisableAutoloadCheckbox = new CheckboxLabel(this);
		DontAskAgainCheckbox = new CheckboxLabel(this);
		LightsCheckbox = new CheckboxLabel(this);
		BrightmapsCheckbox = new CheckboxLabel(this);
		WidescreenCheckbox = new CheckboxLabel(this);
		PlayButton = new PushButton(this);
		ExitButton = new PushButton(this);
		GamesList = new ListView(this);
		Choices = new Dropdown(this);

		SetWindowBackground(Colorf::fromRgba8(51, 51, 51));
		SetWindowBorderColor(Colorf::fromRgba8(51, 51, 51));
		SetWindowCaptionColor(Colorf::fromRgba8(33, 33, 33));
		SetWindowCaptionTextColor(Colorf::fromRgba8(226, 223, 219));
		SetWindowTitle("VKDoom Launcher");

		WelcomeLabel->SetText("Welcome to VKDoom");
		VersionLabel->SetText("Version 0xdeadbabe.");
		SelectLabel->SetText("Select which game file (IWAD) to run.");
		PlayButton->SetText("Play Game");
		ExitButton->SetText("Exit");

		ExitButton->OnClick = []{
			DisplayWindow::ExitLoop();
		};

		GeneralLabel->SetText("General");
		ExtrasLabel->SetText("Extra Graphics");
		FullscreenCheckbox->SetText("Fullscreen");
		DisableAutoloadCheckbox->SetText("Disable autoload");
		DontAskAgainCheckbox->SetText("Don't ask me again");
		LightsCheckbox->SetText("Lights");
		BrightmapsCheckbox->SetText("Brightmaps");
		WidescreenCheckbox->SetText("Widescreen");

		Choices->SetMaxDisplayItems(2);
		Choices->AddItem("First");
		Choices->AddItem("Second");
		Choices->AddItem("Third");
		Choices->AddItem("Fourth");
		Choices->AddItem("Fifth");
		Choices->AddItem("Sixth");

		Choices->OnChanged = [this](int index) {
			std::cout << "Selected " << index << ":" << Choices->GetItem(index) << std::endl;
		};

		try
		{
			auto filedata = ReadAllBytes("banner.png");
			std::vector<unsigned char> pixels;
			unsigned long width = 0, height = 0;
			int result = decodePNG(pixels, width, height, (const unsigned char*)filedata.data(), filedata.size(), true);
			if (result == 0)
			{
				Logo->SetImage(Image::Create(width, height, ImageFormat::R8G8B8A8, pixels.data()));
			}
		}
		catch (...)
		{
		}
	}

	void OnGeometryChanged() override
	{
		double y = 0.0;

		Logo->SetFrameGeometry(0.0, y, GetWidth(), Logo->GetPreferredHeight());
		y += Logo->GetPreferredHeight();

		y += 10.0;

		WelcomeLabel->SetFrameGeometry(20.0, y, GetWidth() - 40.0, WelcomeLabel->GetPreferredHeight());
		y += WelcomeLabel->GetPreferredHeight();

		VersionLabel->SetFrameGeometry(20.0, y, GetWidth() - 40.0, VersionLabel->GetPreferredHeight());
		y += VersionLabel->GetPreferredHeight();

		y += 10.0;

		SelectLabel->SetFrameGeometry(20.0, y, GetWidth() - 40.0 - Choices->GetPreferredWidth(), SelectLabel->GetPreferredHeight());
		y += SelectLabel->GetPreferredHeight();

		Choices->SetFrameGeometry(GetWidth() - 20.0 - Choices->GetPreferredWidth(), y-Choices->GetPreferredHeight(), Choices->GetPreferredWidth(), Choices->GetPreferredHeight());

		double listViewTop = y + 10.0;

		y = GetHeight() - 15.0 - PlayButton->GetPreferredHeight();
		PlayButton->SetFrameGeometry(20.0, y, 120.0, PlayButton->GetPreferredHeight());
		ExitButton->SetFrameGeometry(GetWidth() - 20.0 - 120.0, y, 120.0, PlayButton->GetPreferredHeight());

		y -= 20.0;

		double panelWidth = 150.0;
		y -= DontAskAgainCheckbox->GetPreferredHeight();
		DontAskAgainCheckbox->SetFrameGeometry(20.0, y, 190.0, DontAskAgainCheckbox->GetPreferredHeight());
		WidescreenCheckbox->SetFrameGeometry(GetWidth() - 20.0 - panelWidth, y, panelWidth, WidescreenCheckbox->GetPreferredHeight());

		y -= DisableAutoloadCheckbox->GetPreferredHeight();
		DisableAutoloadCheckbox->SetFrameGeometry(20.0, y, 190.0, DisableAutoloadCheckbox->GetPreferredHeight());
		BrightmapsCheckbox->SetFrameGeometry(GetWidth() - 20.0 - panelWidth, y, panelWidth, BrightmapsCheckbox->GetPreferredHeight());

		y -= FullscreenCheckbox->GetPreferredHeight();
		FullscreenCheckbox->SetFrameGeometry(20.0, y, 190.0, FullscreenCheckbox->GetPreferredHeight());
		LightsCheckbox->SetFrameGeometry(GetWidth() - 20.0 - panelWidth, y, panelWidth, LightsCheckbox->GetPreferredHeight());

		y -= GeneralLabel->GetPreferredHeight();
		GeneralLabel->SetFrameGeometry(20.0, y, 190.0, GeneralLabel->GetPreferredHeight());
		ExtrasLabel->SetFrameGeometry(GetWidth() - 20.0 - panelWidth, y, panelWidth, ExtrasLabel->GetPreferredHeight());

		double listViewBottom = y - 10.0;
		GamesList->SetFrameGeometry(20.0, listViewTop, GetWidth() - 40.0, std::max(listViewBottom - listViewTop, 0.0));
	}

	ImageBox* Logo = nullptr;
	TextLabel* WelcomeLabel = nullptr;
	TextLabel* VersionLabel = nullptr;
	TextLabel* SelectLabel = nullptr;
	TextLabel* GeneralLabel = nullptr;
	TextLabel* ExtrasLabel = nullptr;
	CheckboxLabel* FullscreenCheckbox = nullptr;
	CheckboxLabel* DisableAutoloadCheckbox = nullptr;
	CheckboxLabel* DontAskAgainCheckbox = nullptr;
	CheckboxLabel* LightsCheckbox = nullptr;
	CheckboxLabel* BrightmapsCheckbox = nullptr;
	CheckboxLabel* WidescreenCheckbox = nullptr;
	PushButton* PlayButton = nullptr;
	PushButton* ExitButton = nullptr;
	ListView* GamesList = nullptr;
	Dropdown* Choices = nullptr;
};

static std::vector<uint8_t> ReadAllBytes(const std::string& filename);

std::vector<SingleFontData> LoadWidgetFontData(const std::string& name)
{
	return {
		{std::move(ReadAllBytes("OpenSans.ttf")), ""}
	};
}

std::vector<uint8_t> LoadWidgetData(const std::string& name)
{
	return ReadAllBytes(name);
}

enum class Backend {
	Default, Win32, SDL2, X11, Wayland
};

int example(Backend backend = Backend::Default)
{
	WidgetTheme::SetTheme(std::make_unique<DarkWidgetTheme>());

	// just for testing backends
	switch (backend)
	{
		case Backend::Default: DisplayBackend::Set(DisplayBackend::TryCreateBackend()); break;
		case Backend::Win32:   DisplayBackend::Set(DisplayBackend::TryCreateWin32());   break;
		case Backend::SDL2:    DisplayBackend::Set(DisplayBackend::TryCreateSDL2());    break;
		case Backend::X11:     DisplayBackend::Set(DisplayBackend::TryCreateX11());     break;
		case Backend::Wayland: DisplayBackend::Set(DisplayBackend::TryCreateWayland()); break;
	}

#if 1
	auto launcher = new LauncherWindow();
	launcher->SetFrameGeometry(100.0, 100.0, 615.0, 668.0);
	launcher->Show();
#else
	auto mainwindow = new MainWindow();
	auto textedit = new TextEdit(mainwindow);
	textedit->SetText(R"(
#version 460

in vec4 AttrPos;
in vec4 AttrColor;
out vec4 Color;

void main()
{
	gl_Position = AttrPos;
	Color = AttrColor;
}
)");
	mainwindow->SetWindowTitle("ZWidget Example");
	mainwindow->SetFrameGeometry(100.0, 100.0, 1700.0, 900.0);
	mainwindow->SetCentralWidget(textedit);
	textedit->SetFocus();
	mainwindow->Show();
#endif

	DisplayWindow::RunLoop();

	return 0;
}

#ifdef WIN32

#include <Windows.h>
#include <stdexcept>

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static std::wstring to_utf16(const std::string& str)
{
	if (str.empty()) return {};
	int needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
	if (needed == 0)
		throw std::runtime_error("MultiByteToWideChar failed");
	std::wstring result;
	result.resize(needed);
	needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &result[0], (int)result.size());
	if (needed == 0)
		throw std::runtime_error("MultiByteToWideChar failed");
	return result;
}

static std::vector<uint8_t> ReadAllBytes(const std::string& filename)
{
	HANDLE handle = CreateFile(to_utf16(filename).c_str(), FILE_READ_ACCESS, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (handle == INVALID_HANDLE_VALUE)
		throw std::runtime_error("Could not open " + filename);

	LARGE_INTEGER fileSize;
	BOOL result = GetFileSizeEx(handle, &fileSize);
	if (result == FALSE)
	{
		CloseHandle(handle);
		throw std::runtime_error("GetFileSizeEx failed");
	}

	std::vector<uint8_t> buffer(fileSize.QuadPart);

	DWORD bytesRead = 0;
	result = ReadFile(handle, buffer.data(), (DWORD)buffer.size(), &bytesRead, nullptr);
	if (result == FALSE || bytesRead != buffer.size())
	{
		CloseHandle(handle);
		throw std::runtime_error("ReadFile failed");
	}

	CloseHandle(handle);

	return buffer;
}


int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	example();
}

#else

#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>

static std::vector<uint8_t> ReadAllBytes(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file)
		throw std::runtime_error("ReadFile failed");

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<uint8_t> buffer(size);
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		throw std::runtime_error("ReadFile failed ");

	return buffer;
}

int main(int argc, const char** argv)
{
	std::string backendStr = argc > 1? argv[1]: "";
	std::transform(backendStr.begin(), backendStr.end(), backendStr.begin(),
		[](unsigned char c){ return std::tolower(c); });

	Backend backend = Backend::Default;

	if (backendStr == "sdl2")    backend = Backend::SDL2;
	if (backendStr == "x11")     backend = Backend::X11;
	if (backendStr == "wayland") backend = Backend::Wayland;
	if (backendStr == "win32")   backend = Backend::Win32; // lol

	example(backend);
}

#endif
