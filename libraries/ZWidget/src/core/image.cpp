
#include "core/image.h"
#include "core/resourcedata.h"
#include "picopng/picopng.h"
#include "nanosvg/nanosvg.h"
#include "nanosvg/nanosvgrast.h"
#include <cstring>
#include <stdexcept>

class ImageImpl : public Image
{
public:
	ImageImpl(int width, int height, ImageFormat format, const void* data) : Width(width), Height(height), Format(format)
	{
		Data = std::make_unique<uint32_t[]>(width * height);
		if (data)
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

std::shared_ptr<Image> Image::LoadResource(const std::string& resourcename, double dpiscale)
{
	size_t extensionpos = resourcename.find_last_of("./\\");
	if (extensionpos == std::string::npos || resourcename[extensionpos] != '.')
		throw std::runtime_error("Unsupported image format");
	std::string extension = resourcename.substr(extensionpos + 1);
	for (char& c : extension)
	{
		if (c >= 'A' && c <= 'Z')
			c = c - 'A' + 'a';
	}

	if (extension == "png")
	{
		auto filedata = LoadWidgetImageData(resourcename);

		std::vector<unsigned char> pixels;
		unsigned long width = 0, height = 0;
		int result = decodePNG(pixels, width, height, (const unsigned char*)filedata.data(), filedata.size(), true);
		if (result != 0)
			throw std::runtime_error("Could not decode PNG file");

		return Image::Create(width, height, ImageFormat::R8G8B8A8, pixels.data());
	}
	else if (extension == "svg")
	{
		auto filedata = LoadWidgetImageData(resourcename);
		filedata.push_back(0);

		NSVGimage* svgimage = nsvgParse((char*)filedata.data(), "px", (float)(96.0 * dpiscale));
		if (!svgimage)
			throw std::runtime_error("Could not parse SVG file");

		try
		{
			int width = (int)(svgimage->width * dpiscale);
			int height = (int)(svgimage->height * dpiscale);
			std::shared_ptr<Image> image = Image::Create(width, height, ImageFormat::R8G8B8A8, nullptr);

			NSVGrasterizer* rast = nsvgCreateRasterizer();
			if (!rast)
				throw std::runtime_error("Could not create SVG rasterizer");

			nsvgRasterize(rast, svgimage, 0.0f, 0.0f, (float)dpiscale, (unsigned char*)image->GetData(), width, height, width * 4);

			nsvgDeleteRasterizer(rast);
			nsvgDelete(svgimage);
			return image;
		}
		catch (...)
		{
			nsvgDelete(svgimage);
			throw;
		}
	}
	else
	{
		throw std::runtime_error("Unsupported image format");
	}
}
