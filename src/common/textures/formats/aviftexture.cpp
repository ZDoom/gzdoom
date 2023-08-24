#include "avif/avif.h"

#include "files.h"
#include "filesystem.h"
#include "bitmap.h"
#include "imagehelpers.h"
#include "image.h"
#include "printf.h"

class FAVIFTexture : public FImageSource
{

public:
	FAVIFTexture(int lumpnum, int w, int h, bool transparent);
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
	int CopyPixels(FBitmap *bmp, int conversion, int frame = 0) override;
};


struct AVIFReader
{
	avifIO io;
	std::vector<uint8_t> data;
	FileReader& reader;
};

static avifResult AVIFReaderRead(struct avifIO* io, uint32_t readFlags, uint64_t offset, size_t size, avifROData * out)
{
	AVIFReader* reader = (AVIFReader*)io;
	auto filesize = reader->reader.GetLength();

	if (readFlags != 0)
		return AVIF_RESULT_IO_ERROR;
	
	if (offset > filesize)
		return AVIF_RESULT_IO_ERROR;
	
	if (offset == filesize)
	{
		reader->data.resize(1); // Force a non-null pointer.
		out->data = reader->data.data();
		out->size = 0;
		return AVIF_RESULT_OK;
	}

	uint64_t availableSize = filesize - offset;
	
	if (size > availableSize)
		size = (size_t)availableSize;
	
	reader->reader.Seek(offset, FileReader::SeekSet);
	reader->data.resize(size);
	reader->reader.Read(reader->data.data(), size);
	out->data = reader->data.data();
	out->size = size;
	return AVIF_RESULT_OK;
}

FImageSource *AVIFImage_TryCreate(FileReader &file, int lumpnum)
{
	int width = 0, height = 0;
	bool transparent = true;
	// 'ftyp' is always written first for libavif-encoded images (for now).
	file.Seek(4, FileReader::SeekSet);
	if (file.ReadUInt32() != MAKE_ID('f', 't', 'y', 'p'))
		return nullptr;
	
	if (file.ReadUInt32() != MAKE_ID('a', 'v', 'i', 'f'))
		return nullptr;

	file.Seek(0, FileReader::SeekSet);
	AVIFReader avifReader{{nullptr, AVIFReaderRead, nullptr, (uint64_t)file.GetLength(), false, nullptr}, {}, file};
	auto decoder = avifDecoderCreate();
	if (!decoder)
		return nullptr;

	decoder->ignoreExif = decoder->ignoreXMP = true;

	avifDecoderSetIO(decoder, (avifIO*)&avifReader);
	if (avifDecoderParse(decoder) != AVIF_RESULT_OK)
		return nullptr;
	
	width = decoder->image->width;
	height = decoder->image->height;
	transparent = decoder->alphaPresent;
	avifDecoderDestroy(decoder);
	return new FAVIFTexture(lumpnum, width, height, transparent);
}

FAVIFTexture::FAVIFTexture (int lumpnum, int w, int h, bool transparent)
	: FImageSource(lumpnum)
{
	Width = w;
	Height = h;
	LeftOffset = 0;
	TopOffset = 0;
	bMasked = bTranslucent = transparent;
}

//==========================================================================
//
//
//
//==========================================================================

PalettedPixels FAVIFTexture::CreatePalettedPixels(int conversion, int frame)
{
	FBitmap bitmap;
	bitmap.Create(Width, Height);
	CopyPixels(&bitmap, conversion, frame);
	const uint8_t *data = bitmap.GetPixels();

	uint8_t *dest_p;
	int dest_adv = Height;
	int dest_rew = Width * Height - 1;

	PalettedPixels Pixels(Width*Height);
	dest_p = Pixels.Data();

	bool doalpha = conversion == luminance; 
	// Convert the source image from row-major to column-major format and remap it
	for (int y = Height; y != 0; --y)
	{
		for (int x = Width; x != 0; --x)
		{
			int b = *data++;
			int g = *data++;
			int r = *data++;
			int a = *data++;
			if (a < 128) *dest_p = 0;
			else *dest_p = ImageHelpers::RGBToPalette(doalpha, r, g, b); 
			dest_p += dest_adv;
		}
		dest_p -= dest_rew;
	}
	return Pixels;
}

int FAVIFTexture::CopyPixels(FBitmap *bmp, int conversion, int frame)
{
	auto file = fileSystem.OpenFileReader(SourceLump);
	AVIFReader avifReader{{nullptr, AVIFReaderRead, nullptr, (uint64_t)file.GetLength(), false, nullptr}, {}, file};
	auto decoder = avifDecoderCreate();
	if (!decoder)
		return 0;

	decoder->ignoreExif = decoder->ignoreXMP = true;

	avifDecoderSetIO(decoder, (avifIO*)&avifReader);
	if (avifDecoderParse(decoder) != AVIF_RESULT_OK)
	{
		avifDecoderDestroy(decoder);
		return 0;
	}

	if (avifDecoderNthImage(decoder, 0) != AVIF_RESULT_OK)
	{
		avifDecoderDestroy(decoder);
		return 0;
	}
	
	if (Width != decoder->image->width || Height != decoder->image->height)
	{
		avifDecoderDestroy(decoder);
		return 0;
	}

	avifRGBImage rgbDecoder;
	avifRGBImageSetDefaults(&rgbDecoder, decoder->image);
	avifRGBImageUnpremultiplyAlpha(&rgbDecoder);
	rgbDecoder.pixels = bmp->GetPixels();
	rgbDecoder.rowBytes = bmp->GetPitch();
	rgbDecoder.format = AVIF_RGB_FORMAT_BGRA;
	rgbDecoder.depth = 8;
	avifImageYUVToRGB(decoder->image, &rgbDecoder);
	return bMasked ? -1 : 0;
}
