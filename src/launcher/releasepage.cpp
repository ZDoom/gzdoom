/*
** releasepage.cpp
**
** Release notes tab of launcher
**
**---------------------------------------------------------------------------
**
** Copyright 2025 Marcus Minhorst
** Copyright 2025 GZDoom Maintainers and Contributors
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/
**
**---------------------------------------------------------------------------
**
*/

#include <cstring>

#include <rapidxml/rapidxml.hpp>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>
#include <zwidget/widgets/textedit/textedit.h>

#include "findfile.h"
#include "gameconfigfile.h"
#include "gstrings.h"
#include "i_interface.h"
#include "launcherwindow.h"
#include "name.h"
#include "releasepage.h"
#include "version.h"
#include "zstring.h"

ReleasePage::ReleasePage(LauncherWindow* launcher, const FStartupSelectionInfo& info) : Widget(nullptr), Launcher(launcher)
{
	ShowThis = new CheckboxLabel(this);
	Notes = new TextEdit(this);

	auto text = GetReleaseNotes();

	Notes->SetText(text.GetChars());

	ShowThis->SetChecked(info.notifyNewRelease);
}

void ReleasePage::SetValues(FStartupSelectionInfo& info) const
{
	info.notifyNewRelease = ShowThis->GetChecked();
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


FString ReleasePage::_ParseReleaseNotes(rapidxml::xml_document<> &doc)
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

// Ensure you free returned pointer
char * ReleasePage::_OpenRealeaseNotes()
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

//==========================================================================
//
// Extract release notes from gzdoom.pk3
//
//==========================================================================
FString ReleasePage::GetReleaseNotes()
{
	// we need to be free
	char * text = _OpenRealeaseNotes();

	rapidxml::xml_document<> doc;
	if (text) doc.parse<rapidxml::parse_default>(text);
	FString content = _ParseReleaseNotes(doc);

	free(text);

	return content;
}
