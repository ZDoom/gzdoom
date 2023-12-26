
#include "core/image.h"
#include <cstring>

class ImageImpl : public Image
{
public:
	ImageImpl(int width, int height, ImageFormat format, const void* data) : Width(width), Height(height), Format(format)
	{
		Data = std::make_unique<uint32_t[]>(width * height);
		memcpy(Data.get(), data, width * height * sizeof(uint32_t));
	}

	int GetWidth() const override
	{
		return Width;
	}

	int GetHeight() const override
	{
		return Height;
	}

	ImageFormat GetFormat() const override
	{
		return Format;
	}

	void* GetData() const override
	{
		return Data.get();
	}

	int Width = 0;
	int Height = 0;
	ImageFormat Format = {};
	std::unique_ptr<uint32_t[]> Data;
};

std::shared_ptr<Image> Image::Create(int width, int height, ImageFormat format, const void* data)
{
	return std::make_shared<ImageImpl>(width, height, format, data);
}
