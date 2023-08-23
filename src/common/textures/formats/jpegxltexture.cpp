/*
** jpegxltexture.cpp
** Texture class for JPEG XL images
**
**---------------------------------------------------------------------------
** Copyright 2023 Cacodemon345
** All rights reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**---------------------------------------------------------------------------
**
**
*/

#include <jxl/decode_cxx.h>

#include "files.h"
#include "filesystem.h"
#include "bitmap.h"
#include "imagehelpers.h"
#include "image.h"

class FJPEGXLTexture : public FImageSource
{

public:
	FJPEGXLTexture (int lumpnum, int w, int h, int xoffset, int yoffset, bool transparent = true);
	PalettedPixels CreatePalettedPixels(int conversion) override;
	int CopyPixels(FBitmap *bmp, int conversion) override;
};

FImageSource *JPEGXLImage_TryCreate(FileReader & file, int lumpnum)
{
    JxlBasicInfo info;
    file.Seek(0, FileReader::SeekSet);
    auto array = file.Read();
    int width = 0, height = 0, xoffset = 0, yoffset = 0;
    // TODO: Revisit this again when the box decoding API is well understood.
    (void)xoffset;
    (void)yoffset;

    auto jxlDecoder = JxlDecoderMake(nullptr);
    if (!jxlDecoder)
    {
        return nullptr;
    }

    auto jxlDecoderPtr = jxlDecoder.get();
    if (JxlDecoderSubscribeEvents(jxlDecoderPtr, JXL_DEC_BASIC_INFO) != JXL_DEC_SUCCESS)
    {
        return nullptr;
    }
    JxlDecoderSetInput(jxlDecoderPtr, array.data(), array.size());
    JxlDecoderCloseInput(jxlDecoderPtr);

    for (;;)
    {
        JxlDecoderStatus status = JxlDecoderProcessInput(jxlDecoderPtr);

        switch (status)
        {
            case JXL_DEC_ERROR:
                return nullptr;
            
            case JXL_DEC_SUCCESS:
            case JXL_DEC_BASIC_INFO:
            {
                if (JxlDecoderGetBasicInfo(jxlDecoderPtr, &info) != JXL_DEC_SUCCESS)
                    return nullptr;
                
                width = info.xsize;
                height = info.ysize;
                if (width && height)
                    return new FJPEGXLTexture(lumpnum, width, height, xoffset, yoffset, !!info.alpha_bits);
                else
                    return nullptr;
                break;
            }
        }
    }

    return nullptr;
}

FJPEGXLTexture::FJPEGXLTexture (int lumpnum, int w, int h, int xoffset, int yoffset, bool transparent)
	: FImageSource(lumpnum)
{
    Width = w;
    Height = h;
    LeftOffset = xoffset;
    TopOffset = yoffset;
    if (!transparent)
        bMasked = bTranslucent = false;
}

PalettedPixels FJPEGXLTexture::CreatePalettedPixels(int conversion)
{
	FBitmap bitmap;
	bitmap.Create(Width, Height);
	CopyPixels(&bitmap, conversion);
	const uint8_t *data = bitmap.GetPixels();

	uint8_t *dest_p;
	int dest_adv = Height;
	int dest_rew = Width * Height - 1;

	PalettedPixels Pixels(Width * Height);
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
			if (a < 128)
				*dest_p = 0;
			else
				*dest_p = ImageHelpers::RGBToPalette(doalpha, r, g, b);
			dest_p += dest_adv;
		}
		dest_p -= dest_rew;
	}
	return Pixels;
}

int FJPEGXLTexture::CopyPixels(FBitmap *bmp, int conversion)
{
    JxlBasicInfo info;
    int width = 0, height = 0;
    uint8_t* pixels = nullptr;
    JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    auto jxlDecoder = JxlDecoderMake(nullptr);
    if (!jxlDecoder)
    {
        return 0;
    }

    auto jxlDecoderPtr = jxlDecoder.get();
    if (JxlDecoderSubscribeEvents(jxlDecoderPtr, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS)
    {
        return 0;
    }

    auto lump = fileSystem.OpenFileReader(SourceLump);
	auto array = lump.Read();
    JxlDecoderSetInput(jxlDecoderPtr, array.data(), array.size());
    JxlDecoderCloseInput(jxlDecoderPtr);

    for (;;)
    {
        JxlDecoderStatus status = JxlDecoderProcessInput(jxlDecoderPtr);

        switch (status)
        {
            case JXL_DEC_ERROR:
                return 0;

            case JXL_DEC_BASIC_INFO:
            {
                if (JxlDecoderGetBasicInfo(jxlDecoderPtr, &info) != JXL_DEC_SUCCESS)
                    return 0;
                
                width = info.xsize;
                height = info.ysize;
                if (width != Width && height != Height)
                    return 0;
                break;
            }

            case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
            {
                size_t bufferSize = 0;
                if (JxlDecoderImageOutBufferSize(jxlDecoderPtr, &format, &bufferSize) != JXL_DEC_SUCCESS)
                    return 0;

                if (bufferSize != (Width * Height * 4))
                    return 0;
                
                pixels = new uint8_t[Width * Height * 4];

                if (JxlDecoderSetImageOutBuffer(jxlDecoderPtr, &format, pixels, bufferSize) != JXL_DEC_SUCCESS)
                    return 0;
                break;
            }

            case JXL_DEC_FULL_IMAGE:
            {
                break;
            }

            case JXL_DEC_SUCCESS:
                {
                    bmp->CopyPixelDataRGB(0, 0, pixels, Width, Height, 4, Width * 4, 0, CF_RGBA); 
                    
                    return bMasked ? -1 : 0;
                }
        }
    }
}
