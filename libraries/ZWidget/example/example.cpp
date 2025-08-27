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
#include <zwidget/widgets/tabwidget/tabwidget.h>

// ************************************************************
// Prototypes
// ************************************************************

static std::vector<uint8_t> ReadAllBytes(const std::string& filename);

class LauncherWindowTab1 : public Widget
{
public:
	LauncherWindowTab1(Widget parent);
	void OnGeometryChanged() override;
private:
	TextEdit* Text = nullptr;
};

class LauncherWindowTab2 : public Widget
{
public:
	LauncherWindowTab2(Widget parent);
	void OnGeometryChanged() override;
private:
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
	ListView* GamesList = nullptr;
};

class LauncherWindowTab3 : public Widget
{
public:
	LauncherWindowTab3(Widget parent);
	void OnGeometryChanged() override;
private:
	TextLabel* Label = nullptr;
	Dropdown* Choices = nullptr;
	PushButton* Popup = nullptr;
};

class LauncherWindow : public Widget
{
public:
	LauncherWindow();
private:
	void OnClose() override;
	void OnGeometryChanged() override;

	ImageBox* Logo = nullptr;
	TabWidget* Pages = nullptr;
	PushButton* ExitButton = nullptr;

	LauncherWindowTab1* Tab1 = nullptr;
	LauncherWindowTab2* Tab2 = nullptr;
	LauncherWindowTab3* Tab3 = nullptr;
};

// ************************************************************
// UI implementation
// ************************************************************

LauncherWindow::LauncherWindow(): Widget(nullptr, WidgetType::Window)
{
	SetWindowTitle("ZWidget Demo");

	Logo = new ImageBox(this);
	ExitButton = new PushButton(this);
	Pages = new TabWidget(this);

	Tab1 = new LauncherWindowTab1(this);
	Tab2 = new LauncherWindowTab2(this);
	Tab3 = new LauncherWindowTab3(this);

	Pages->AddTab(Tab1, "Welcome");
	Pages->AddTab(Tab2, "VKDoom");
	Pages->AddTab(Tab3, "ZWidgets");

	ExitButton->SetText("Exit");

	ExitButton->OnClick = []{
		DisplayWindow::ExitLoop();
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

void LauncherWindow::OnGeometryChanged()
{
	double y = 0, h;

	h = Logo->GetPreferredHeight();
	Logo->SetFrameGeometry(0, y, GetWidth(), h);
	y += h;

	h = GetHeight() - y - ExitButton->GetPreferredHeight() - 40;
	Pages->SetFrameGeometry(0, y, GetWidth(), h);
	y += h + 20;

	ExitButton->SetFrameGeometry(GetWidth() - 20 - 120, y, 120, ExitButton->GetPreferredHeight());
}

void LauncherWindow::OnClose()
{
	DisplayWindow::ExitLoop();
}

LauncherWindowTab1::LauncherWindowTab1(Widget parent): Widget(nullptr)
{
	Text = new TextEdit(this);

	Text->SetText(
		"Welcome to VKDoom\n\n"
		"Click the tabs to look at other widgets\n\n"
		"Also, this text is editable\n"
	);
}

void LauncherWindowTab1::OnGeometryChanged()
{
	Text->SetFrameGeometry(0, 10, GetWidth(), GetHeight());
}

LauncherWindowTab2::LauncherWindowTab2(Widget parent): Widget(nullptr)
{
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
	GamesList = new ListView(this);

	WelcomeLabel->SetText("Welcome to VKDoom");
	VersionLabel->SetText("Version 0xdeadbabe.");
	SelectLabel->SetText("Select which game file (IWAD) to run.");

	GamesList->AddItem("Doom");
	GamesList->AddItem("Doom 2: Electric Boogaloo");
	GamesList->AddItem("Doom 3D");
	GamesList->AddItem("Doom 4: The Quest for Peace");
	GamesList->AddItem("Doom on Ice");
	GamesList->AddItem("The Doom");
	GamesList->AddItem("Doom 2");

	GeneralLabel->SetText("General");
	ExtrasLabel->SetText("Extra Graphics");
	FullscreenCheckbox->SetText("Fullscreen");
	DisableAutoloadCheckbox->SetText("Disable autoload");
	DontAskAgainCheckbox->SetText("Don't ask me again");
	LightsCheckbox->SetText("Lights");
	BrightmapsCheckbox->SetText("Brightmaps");
	WidescreenCheckbox->SetText("Widescreen");
}

void LauncherWindowTab2::OnGeometryChanged()
{
	double y = 0, h;

	h = WelcomeLabel->GetPreferredHeight();
	WelcomeLabel->SetFrameGeometry(20, y, GetWidth() - 40, h);
	y += h;

	h = VersionLabel->GetPreferredHeight();
	VersionLabel->SetFrameGeometry(20, y, GetWidth() - 40, h);
	y += h + 10;

	h = SelectLabel->GetPreferredHeight();
	SelectLabel->SetFrameGeometry(20, y, GetWidth() - 40, h);
	y += h;

	double listViewTop = y + 10, listViewBottom;

	y = GetHeight();

	h = DontAskAgainCheckbox->GetPreferredHeight();
	y -= h;
	DontAskAgainCheckbox->SetFrameGeometry(20, y, 190, h);
	WidescreenCheckbox->SetFrameGeometry(GetWidth() - 170, y, 150, WidescreenCheckbox->GetPreferredHeight());

	h = DisableAutoloadCheckbox->GetPreferredHeight();
	y -= h;
	DisableAutoloadCheckbox->SetFrameGeometry(20, y, 190, h);
	BrightmapsCheckbox->SetFrameGeometry(GetWidth() - 170, y, 150, BrightmapsCheckbox->GetPreferredHeight());

	h = FullscreenCheckbox->GetPreferredHeight();
	y -= h;
	FullscreenCheckbox->SetFrameGeometry(20, y, 190, h);
	LightsCheckbox->SetFrameGeometry(GetWidth() - 170, y, 150, LightsCheckbox->GetPreferredHeight());

	h = GeneralLabel->GetPreferredHeight();
	y -= h;
	GeneralLabel->SetFrameGeometry(20, y, 190, GeneralLabel->GetPreferredHeight());
	ExtrasLabel->SetFrameGeometry(GetWidth() - 170, y, 150, ExtrasLabel->GetPreferredHeight());

	listViewBottom = y - 10;
	GamesList->SetFrameGeometry(20, listViewTop, GetWidth() - 40, std::max<double>(listViewBottom - listViewTop, 0));
}

LauncherWindowTab3::LauncherWindowTab3(Widget parent): Widget(nullptr)
{
	Label = new TextLabel(this);
	Choices = new Dropdown(this);
	Popup = new PushButton(this);

	Label->SetText("Oh my, even more widgets");
	Popup->SetText("Click me.");

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

	Popup->OnClick = []{
		std::cout << "TODO: open popup" << std::endl;
	};
}

void LauncherWindowTab3::OnGeometryChanged()
{
	double y = 0, h;

	y += 10;

	h = Label->GetPreferredHeight();
	Label->SetFrameGeometry(20, y, GetWidth() - 40, h);
	y += h + 10;

	h = Choices->GetPreferredHeight();
	Choices->SetFrameGeometry(20, y, Choices->GetPreferredWidth(), h);
	y += h + 10;

	h = Popup->GetPreferredHeight();
	Popup->SetFrameGeometry(20, y, 120, h);
	y += h;
}

// ************************************************************
// Shared code
// ************************************************************

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

enum class Theme {
	Default, Light, Dark
};

int example(
	Backend backend = Backend::Default,
	Theme theme = Theme::Default
)
{
	// just for testing themes
	switch (theme)
	{
		case Theme::Default: WidgetTheme::SetTheme(std::make_unique<DarkWidgetTheme>()); break;
		case Theme::Dark:    WidgetTheme::SetTheme(std::make_unique<DarkWidgetTheme>()); break;
		case Theme::Light:   WidgetTheme::SetTheme(std::make_unique<LightWidgetTheme>()); break;
	}

	// just for testing backends
	switch (backend)
	{
		case Backend::Default: DisplayBackend::Set(DisplayBackend::TryCreateBackend()); break;
		case Backend::Win32:   DisplayBackend::Set(DisplayBackend::TryCreateWin32());   break;
		case Backend::SDL2:    DisplayBackend::Set(DisplayBackend::TryCreateSDL2());    break;
		case Backend::X11:     DisplayBackend::Set(DisplayBackend::TryCreateX11());     break;
		case Backend::Wayland: DisplayBackend::Set(DisplayBackend::TryCreateWayland()); break;
	}

	auto launcher = new LauncherWindow();
	launcher->SetFrameGeometry(100.0, 100.0, 615.0, 668.0);
	launcher->Show();

	DisplayWindow::RunLoop();

	return 0;
}

// ************************************************************
// Platform-specific code
// ************************************************************

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
	Backend backend = Backend::Default;
	Theme theme = Theme::Default;

	for (auto i = 1; i < argc; i++)
	{
		std::string s = argv[i];
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c){ return std::tolower(c); });

		if (s == "light") { theme = Theme::Light; continue; }
		if (s == "dark")  { theme = Theme::Dark;  continue; }

		if (s == "sdl2")    { backend = Backend::SDL2;    continue; }
		if (s == "x11")     { backend = Backend::X11;     continue; }
		if (s == "wayland") { backend = Backend::Wayland; continue; }
		if (s == "win32")   { backend = Backend::Win32;   continue; } // lol
	}

	example(backend, theme);
}

#endif
