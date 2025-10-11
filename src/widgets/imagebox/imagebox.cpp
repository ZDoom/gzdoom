
#include "widgets/imagebox/imagebox.h"

ImageBox::ImageBox(Widget* parent) : Widget(parent)
{
}

double ImageBox::GetPreferredWidth() const
{
	if (image)
		return (double)image->GetWidth();
	else
		return 0.0;
}

double ImageBox::GetPreferredHeight() const
{
	if (image)
		return (double)image->GetHeight();
	else
		return 0.0;
}

void ImageBox::SetImage(std::shared_ptr<Image> newImage)
{
	if (image != newImage)
	{
		image = newImage;
		Update();
	}
}

void ImageBox::SetImageMode(ImageBoxMode newMode)
{
	if (mode != newMode)
	{
		mode = newMode;
		Update();
	}
}

void ImageBox::OnPaint(Canvas* canvas)
{
	if (image)
	{
		if (mode == ImageBoxMode::Center)
		{
			canvas->drawImage(image, Point((GetWidth() - (double)image->GetWidth()) * 0.5, (GetHeight() - (double)image->GetHeight()) * 0.5));
		}
		else if (mode == ImageBoxMode::Contain)
		{
			double bw = GetWidth();
			double bh = GetHeight();
			double iw = image->GetWidth();
			double ih = image->GetHeight();
			double xscale = bw / iw;
			double yscale = bh / ih;
			double scale = std::min(xscale, yscale);
			canvas->drawImage(image, Rect::xywh((bw - iw * scale) * 0.5, (bh - ih * scale) * 0.5, iw * scale, ih * scale));
		}
		else if (mode == ImageBoxMode::Cover)
		{
			double bw = GetWidth();
			double bh = GetHeight();
			double iw = image->GetWidth();
			double ih = image->GetHeight();
			double xscale = bw / iw;
			double yscale = bh / ih;
			double scale = std::max(xscale, yscale);
			canvas->drawImage(image, Rect::xywh((bw - iw * scale) * 0.5, (bh - ih * scale) * 0.5, iw * scale, ih * scale));
		}
	}
}
