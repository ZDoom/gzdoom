#include <cstring>

#include <zwidget/widgets/textedit/textedit.h>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>
#include <rapidxml/rapidxml.hpp>

#include "releasepage.h"
#include "findfile.h"
#include "gameconfigfile.h"
#include "launcherwindow.h"
#include "i_interface.h"
#include "name.h"
#include "version.h"
#include "zstring.h"
#include "gstrings.h"

FString GetReleaseNotes(rapidxml::xml_document<> &doc)
{
	// braindead html to plaintext parser

	auto fallback = "Unable to parse release notes";
	auto unknown = "Unable to parse release notes";

	// traverse to first (latest) release node
	auto release = doc.first_node("component");
	if (!release) return fallback;
	release = release->first_node("releases");
	if (!release) return fallback;
	release = release->first_node("release");
	if (!release) return fallback;
	auto description = release->first_node("description");
	if (!description) return fallback;

	auto version = release->first_attribute("version");
	auto date = release->first_attribute("date");
	auto url = release->first_node("url");

	auto versionStr = version? version->value(): unknown;
	auto dateStr = date? date->value(): unknown;
	auto header = FStringf("GZDoom version %s, released %s\n", versionStr, dateStr);
	auto footer = url? FStringf("For more details see: %s", url->value()): "";
	FString text;

	auto append = [&text](FName type, char * value)
	{
		if (type == "p")
		{
			text.AppendFormat("%s\n\n", value);
		}
		else if (type == "li")
		{
			text.AppendFormat(" - %s\n", value);
		}
		else if (type == "hr")
		{
			text.AppendFormat("---\n");
		}
		else
		{
			text.AppendFormat("%s", value);
		}
	};

	auto node = description;
	while (node)
	{
		// Only append the value if it's a data/text node with content
		if (node->type() == rapidxml::node_data && node->value_size() > 0)
		{
			append(node->parent()->name(), node->value());
		}

		if (node->first_node())
		{
			node = node->first_node();
		}
		else if (node->next_sibling())
		{
			node = node->next_sibling();
		}
		else
		{
			while (node->parent() && node->parent() != description && !node->parent()->next_sibling())
				node = node->parent();

			node = (node->parent() != description)
				? node->parent()->next_sibling()
				: nullptr;
		}
	}

	return FStringf("%s\n%s\n%s", header.GetChars(), text.GetChars(), footer.GetChars());
}

//==========================================================================
//
// Open release notes as writable string
//
// Ensure you free returned pointer
//
//==========================================================================
char * OpenRealeaseNotes()
{
	auto wad = BaseFileSearch(BASEWAD, NULL, true, GameConfig);
	if (!wad) return nullptr;

	// we need to be free
	auto resf = FResourceFile::OpenResourceFile(wad);
	if (!resf) return nullptr;

	char * notes = nullptr;
	auto lump = resf->FindEntry("meta.xml"); // created from org.zdoom.GZDoom.metainfo.xml during build

	if (lump >= 0)
	{
		auto data = resf->Read(lump);

		// needs to be freed by caller
		notes = (char *)calloc(data.size(), sizeof(char));

		if (notes) strncpy(notes, data.string(), data.size());
	}

	delete resf;

	return notes;
}

ReleasePage::ReleasePage(LauncherWindow* launcher, const FStartupSelectionInfo& info) : Widget(nullptr), Launcher(launcher)
{
	ShowThis = new CheckboxLabel(this);
	Notes = new TextEdit(this);

	char * text = OpenRealeaseNotes();

	rapidxml::xml_document<> doc;
	if (text) doc.parse<rapidxml::parse_default>(text);
	FString content = GetReleaseNotes(doc);

	free(text);

	Notes->SetText(content.GetChars());

	ShowThis->SetChecked(info.notifyNewRelease);
}

void ReleasePage::SetValues(FStartupSelectionInfo& info) const
{
}

void ReleasePage::UpdateLanguage()
{
	ShowThis->SetText(GStrings.GetString("PICKER_SHOWTHIS"));
}

void ReleasePage::OnGeometryChanged()
{
	double y = 0.0;
	double w = GetWidth();
	double h = GetHeight();

	Notes->SetFrameGeometry(0.0, y, w, h - ShowThis->GetPreferredHeight());
	y += h - ShowThis->GetPreferredHeight();

	ShowThis->SetFrameGeometry(0.0, y, w, ShowThis->GetPreferredHeight());
	y += ShowThis->GetPreferredHeight();

	Launcher->UpdatePlayButton();
}
