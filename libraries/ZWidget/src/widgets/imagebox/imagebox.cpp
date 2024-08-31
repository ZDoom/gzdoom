
#include "widgets/imagebox/imagebox.h"

ImageBox::ImageBox(Widget* parent) : Widget(parent)
{
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

void ImageBox::OnPaint(Canvas* canvas)
{
	if (image)
	{
		canvas->drawImage(image, Point((GetWidth() - (double)image->GetWidth()) * 0.5, (GetHeight() - (double)image->GetHeight()) * 0.5));
	}
}
