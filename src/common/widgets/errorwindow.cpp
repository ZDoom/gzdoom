
#include "errorwindow.h"
#include "version.h"
#include "v_font.h"
#include "printf.h"
#include <zwidget/core/image.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/scrollbar/scrollbar.h>
#include <zwidget/systemdialogs/save_file_dialog.h>
#include <miniz.h>

bool ErrorWindow::ExecModal(const std::string& text, const std::string& log, std::vector<uint8_t> minidump)
{
	Size screenSize = GetScreenSize();
	double windowWidth = 1200.0;
	double windowHeight = 700.0;

	auto window = std::make_unique<ErrorWindow>(std::move(minidump));
	window->SetText(text, log);
	window->SetFrameGeometry((screenSize.width - windowWidth) * 0.5, (screenSize.height - windowHeight) * 0.5, windowWidth, windowHeight);
	window->Show();

	DisplayWindow::RunLoop();

	return window->Restart;
}

ErrorWindow::ErrorWindow(std::vector<uint8_t> initminidump) : Widget(nullptr, WidgetType::Window), minidump(std::move(initminidump))
{
	FStringf caption("Fatal Error - " GAMENAME " %s (%s)", GetVersionString(), GetGitTime());
	SetWindowTitle(caption.GetChars());
	SetWindowBackground(Colorf::fromRgba8(51, 51, 51));
	SetWindowBorderColor(Colorf::fromRgba8(51, 51, 51));
	SetWindowCaptionColor(Colorf::fromRgba8(33, 33, 33));
	SetWindowCaptionTextColor(Colorf::fromRgba8(226, 223, 219));

	LogView = new LogViewer(this);
	ClipboardButton = new PushButton(this);
	ClipboardButton->OnClick = [=]() { OnClipboardButtonClicked(); };
	ClipboardButton->SetText("Copy to clipboard");

	if (minidump.empty())
	{
		RestartButton = new PushButton(this);
		RestartButton->OnClick = [=]() { OnRestartButtonClicked(); };
		RestartButton->SetText("Restart");
	}
	else
	{
		SaveReportButton = new PushButton(this);
		SaveReportButton->OnClick = [=]() { OnSaveReportButtonClicked(); };
		SaveReportButton->SetText("Save Report");
	}

	LogView->SetFocus();
}

void ErrorWindow::SetText(const std::string& text, const std::string& log)
{
	LogView->SetText(text, log);

	clipboardtext.clear();
	clipboardtext.reserve(log.size() + text.size() + 100);

	// Strip the color escapes from the log
	const uint8_t* cptr = (const uint8_t*)log.data();
	while (int chr = GetCharFromString(cptr))
	{
		if (chr != TEXTCOLOR_ESCAPE)
		{
			// The bar characters, most commonly used to indicate map changes
			if (chr >= 0x1D && chr <= 0x1F)
			{
				chr = 0x2550;	// Box Drawings Double Horizontal
			}
			clipboardtext += MakeUTF8(chr);
		}
	}

	clipboardtext += "\nExecution could not continue.\n";
	clipboardtext += text;
	clipboardtext += "\n";
}

void ErrorWindow::OnClipboardButtonClicked()
{
	SetClipboardText(clipboardtext);
}

void ErrorWindow::OnRestartButtonClicked()
{
	Restart = true;
	DisplayWindow::ExitLoop();
}

void ErrorWindow::OnSaveReportButtonClicked()
{
	auto dialog = SaveFileDialog::Create(this);
	dialog->AddFilter("Crash Report Zip Files", "*.zip");
	dialog->AddFilter("All Files", "*.*");
	dialog->SetFilename("CrashReport.zip");
	dialog->SetDefaultExtension("zip");
	if (dialog->Show())
	{
		std::string filename = dialog->Filename();

		mz_zip_archive zip = {};
		if (mz_zip_writer_init_heap(&zip, 0, 16 * 1024 * 1024))
		{
			mz_zip_writer_add_mem(&zip, "minidump.dmp", minidump.data(), minidump.size(), MZ_DEFAULT_COMPRESSION);
			mz_zip_writer_add_mem(&zip, "log.txt", clipboardtext.data(), clipboardtext.size(), MZ_DEFAULT_COMPRESSION);
		}
		void* buffer = nullptr;
		size_t buffersize = 0;
		mz_zip_writer_finalize_heap_archive(&zip, &buffer, &buffersize);
		mz_zip_writer_end(&zip);

		std::unique_ptr<FileWriter> f(FileWriter::Open(filename.c_str()));
		if (f)
		{
			f->Write(buffer, buffersize);
			f->Close();
		}
	}
}

void ErrorWindow::OnClose()
{
	Restart = false;
	DisplayWindow::ExitLoop();
}

void ErrorWindow::OnGeometryChanged()
{
	double w = GetWidth();
	double h = GetHeight();

	double y = GetHeight() - 15.0 - ClipboardButton->GetPreferredHeight();
	ClipboardButton->SetFrameGeometry(20.0, y, 170.0, ClipboardButton->GetPreferredHeight());
	if (RestartButton)
		RestartButton->SetFrameGeometry(GetWidth() - 20.0 - 100.0, y, 100.0, RestartButton->GetPreferredHeight());
	else if (SaveReportButton)
		SaveReportButton->SetFrameGeometry(GetWidth() - 20.0 - 100.0, y, 100.0, SaveReportButton->GetPreferredHeight());
	y -= 20.0;

	LogView->SetFrameGeometry(Rect::xywh(0.0, 0.0, w, y));
}

/////////////////////////////////////////////////////////////////////////////

LogViewer::LogViewer(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(8.0, 8.0, 3.0, 8.0);

	scrollbar = new Scrollbar(this);
	scrollbar->FuncScroll = [=]() { OnScrollbarScroll(); };
}

void LogViewer::SetText(const std::string& text, const std::string& log)
{
	lines.clear();

	std::string::size_type start = 0;
	std::string::size_type end = log.find('\n');
	while (end != std::string::npos)
	{
		lines.push_back(CreateLineLayout(log.substr(start, end - start)));
		start = end + 1;
		end = log.find('\n', start);
	}

	lines.push_back(CreateLineLayout(log.substr(start)));

	// Add an empty line as a bit of spacing
	lines.push_back(CreateLineLayout({}));

	SpanLayout layout;
	//layout.AddImage(Image::LoadResource("widgets/erroricon.svg"), -8.0);
	layout.AddText("Execution could not continue.", largefont, Colorf::fromRgba8(255, 170, 170));
	lines.push_back(layout);

	layout.Clear();
	layout.AddText(text, largefont, Colorf::fromRgba8(255, 255, 170));
	lines.push_back(layout);

	scrollbar->SetRanges(0.0, (double)lines.size(), 1.0, 100.0);
	scrollbar->SetPosition((double)lines.size() - 1.0);

	Update();
}

SpanLayout LogViewer::CreateLineLayout(const std::string& text)
{
	SpanLayout layout;

	Colorf curcolor = Colorf::fromRgba8(255, 255, 255);
	std::string curtext;

	const uint8_t* cptr = (const uint8_t*)text.data();
	while (int chr = GetCharFromString(cptr))
	{
		if (chr != TEXTCOLOR_ESCAPE)
		{
			// The bar characters, most commonly used to indicate map changes
			if (chr >= 0x1D && chr <= 0x1F)
			{
				chr = 0x2550;	// Box Drawings Double Horizontal
			}
			curtext += MakeUTF8(chr);
		}
		else
		{
			EColorRange range = V_ParseFontColor(cptr, CR_UNTRANSLATED, CR_YELLOW);
			if (range != CR_UNDEFINED)
			{
				if (!curtext.empty())
					layout.AddText(curtext, font, curcolor);
				curtext.clear();

				PalEntry color = V_LogColorFromColorRange(range);
				curcolor = Colorf::fromRgba8(color.r, color.g, color.b);
			}
		}
	}

	curtext.push_back(' ');
	layout.AddText(curtext, font, curcolor);

	return layout;
}

void LogViewer::OnPaintFrame(Canvas* canvas)
{
	double w = GetFrameGeometry().width;
	double h = GetFrameGeometry().height;
	Colorf bordercolor = Colorf::fromRgba8(100, 100, 100);
	canvas->fillRect(Rect::xywh(0.0, 0.0, w, h), Colorf::fromRgba8(38, 38, 38));
	//canvas->fillRect(Rect::xywh(0.0, 0.0, w, 1.0), bordercolor);
	//canvas->fillRect(Rect::xywh(0.0, h - 1.0, w, 1.0), bordercolor);
	//canvas->fillRect(Rect::xywh(0.0, 0.0, 1.0, h - 0.0), bordercolor);
	//canvas->fillRect(Rect::xywh(w - 1.0, 0.0, 1.0, h - 0.0), bordercolor);
}

void LogViewer::OnPaint(Canvas* canvas)
{
	double width = GetWidth() - scrollbar->GetFrameGeometry().width;
	double y = GetHeight();
	size_t start = std::min((size_t)std::round(scrollbar->GetPosition() + 1.0), lines.size());
	for (size_t i = start; i > 0 && y > 0.0; i--)
	{
		SpanLayout& layout = lines[i - 1];
		layout.Layout(canvas, width);
		layout.SetPosition(Point(0.0, y - layout.GetSize().height));
		layout.DrawLayout(canvas);
		y -= layout.GetSize().height;
	}
}

bool LogViewer::OnMouseWheel(const Point& pos, InputKey key)
{
	if (key == InputKey::MouseWheelUp)
	{
		ScrollUp(4);
	}
	else if (key == InputKey::MouseWheelDown)
	{
		ScrollDown(4);
	}
	return true;
}

void LogViewer::OnKeyDown(InputKey key)
{
	if (key == InputKey::Home)
	{
		scrollbar->SetPosition(0.0f);
		Update();
	}
	if (key == InputKey::End)
	{
		scrollbar->SetPosition(scrollbar->GetMax());
		Update();
	}
	else if (key == InputKey::PageUp)
	{
		ScrollUp(20);
	}
	else if (key == InputKey::PageDown)
	{
		ScrollDown(20);
	}
	else if (key == InputKey::Up)
	{
		ScrollUp(4);
	}
	else if (key == InputKey::Down)
	{
		ScrollDown(4);
	}
}

void LogViewer::OnScrollbarScroll()
{
	Update();
}

void LogViewer::OnGeometryChanged()
{
	double w = GetWidth();
	double h = GetHeight();
	double sw = scrollbar->GetPreferredWidth();
	scrollbar->SetFrameGeometry(Rect::xywh(w - sw, 0.0, sw, h));
}

void LogViewer::ScrollUp(int lines)
{
	scrollbar->SetPosition(std::max(scrollbar->GetPosition() - (double)lines, 0.0));
	Update();
}

void LogViewer::ScrollDown(int lines)
{
	scrollbar->SetPosition(std::min(scrollbar->GetPosition() + (double)lines, scrollbar->GetMax()));
	Update();
}
