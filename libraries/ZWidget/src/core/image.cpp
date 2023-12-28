
#include "core/image.h"
#include "core/resourcedata.h"
#include "picopng/picopng.h"
#include <cstring>
#include <stdexcept>

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

std::shared_ptr<Image> Image::LoadResource(const std::string& resourcename)
{
	auto filedata = LoadWidgetImageData(resourcename);
	std::vector<unsigned char> pixels;
	unsigned long width = 0, height = 0;
	int result = decodePNG(pixels, width, height, (const unsigned char*)filedata.data(), filedata.size(), true);
	if (result != 0)
		throw std::runtime_error("Could not decode PNG file");
	return Image::Create(width, height, ImageFormat::R8G8B8A8, pixels.data());
}
