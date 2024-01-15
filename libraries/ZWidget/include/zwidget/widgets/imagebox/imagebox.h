
#pragma once

#include "../../core/widget.h"
#include "../../core/image.h"

class ImageBox : public Widget
{
public:
	ImageBox(Widget* parent);

	void SetImage(std::shared_ptr<Image> newImage);

	double GetPreferredHeight() const;

protected:
	void OnPaint(Canvas* canvas) override;

private:
	std::shared_ptr<Image> image;
};
