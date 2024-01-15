
#include "launcherbanner.h"
#include "gstrings.h"
#include "version.h"
#include <zwidget/widgets/imagebox/imagebox.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/core/image.h>

LauncherBanner::LauncherBanner(Widget* parent) : Widget(parent)
{
	Logo = new ImageBox(this);
	VersionLabel = new TextLabel(this);
	VersionLabel->SetTextAlignment(TextLabelAlignment::Right);

	Logo->SetImage(Image::LoadResource("widgets/banner.png"));
}

void LauncherBanner::UpdateLanguage()
{
	FString versionText = GStrings("PICKER_VERSION");
	versionText.Substitute("%s", GetVersionString());
	VersionLabel->SetText(versionText.GetChars());
}

double LauncherBanner::GetPreferredHeight() const
{
	return Logo->GetPreferredHeight();
}

void LauncherBanner::OnGeometryChanged()
{
	Logo->SetFrameGeometry(0.0, 0.0, GetWidth(), Logo->GetPreferredHeight());
	VersionLabel->SetFrameGeometry(20.0, GetHeight() - 10.0 - VersionLabel->GetPreferredHeight(), GetWidth() - 40.0, VersionLabel->GetPreferredHeight());
}
