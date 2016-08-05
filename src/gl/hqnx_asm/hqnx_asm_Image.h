//CImage class - loading and saving BMP and TGA files
//----------------------------------------------------------
//Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 2.1 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//Lesser General Public License for more details.
//
//You should have received a copy of the GNU Lesser General Public
//License along with this program; if not, write to the Free Software
//Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

//#ifdef WIN32
//#define DLL __declspec(dllexport)
//#else
#define DLL
//#endif

#include <stdio.h>
#pragma once
#ifdef _MSC_VER
#pragma warning(disable: 4103)
#endif // _MSC_VER
#pragma pack(1)

namespace HQnX_asm
{

typedef struct { unsigned char b, g, r; } _BGR;
typedef struct { unsigned char b, g, r, a; } _BGRA;

class CImage
{
  public:
    DLL CImage();
    DLL ~CImage();

  enum CImageErrors
  {
    eConvUnknownFormat   = 10,
    eConvSourceMemory    = 11,
    eConvDestMemory      = 12,

    eSaveBmpFileOpen     = 20,
    eSaveBmpFileWrite    = 21,
    eSaveBmpSourceMemory = 22,
    eSaveBmpColorDepth   = 23,

    eLoadBmpFileOpen     = 30,
    eLoadBmpFileRead     = 31,
    eLoadBmpBadFormat    = 32,
    eLoadBmpInit         = 33,
    eLoadBmpColorDepth   = 34,

    eSaveTgaFileOpen     = 40,
    eSaveTgaFileWrite    = 41,
    eSaveTgaSourceMemory = 42,
    eSaveTgaColorDepth   = 43,

    eLoadTgaFileOpen     = 50,
    eLoadTgaFileRead     = 51,
    eLoadTgaBadFormat    = 52,
    eLoadTgaInit         = 53,
    eLoadTgaColorDepth   = 54,

    eLoadFilename        = 60,
    eSaveFilename        = 61,
  };

  struct _BMPFILEHEADER
  {
    unsigned short bfType;
    long int       bfSize, bfRes1, bfOffBits; 
  };

  struct _BMPIMAGEHEADEROLD
  {
    long int       biSize;
    unsigned short biWidth, biHeight;
    unsigned short biPlanes, biBitCount;
  };

  struct _BMPIMAGEHEADER
  {
    long int       biSize, biWidth, biHeight;
    unsigned short biPlanes, biBitCount;
    long int       biCompression, biSizeImage;
    long int       biXPelsPerMeter, biYPelsPerMeter;
    long int       biClrUsed, biClrImportant; 
  };

  struct _TGAHEADER
  {
    unsigned char  tiIdentSize;
    unsigned char  tiPaletteIncluded;
    unsigned char  tiImageType;
    unsigned short tiPaletteStart;
    unsigned short tiPaletteSize;
    unsigned char  tiPaletteBpp;
    unsigned short tiX0;
    unsigned short tiY0;
    unsigned short tiXres;
    unsigned short tiYres;
    unsigned char  tiBitPerPixel;
    unsigned char  tiAttrBits;
  };

  public:
    int  DLL Init( int Xres, int Yres, unsigned short BitPerPixel );
    int  DLL SetImage(unsigned char *img, int width, int height, int bpp);
    int  DLL Destroy();
    int  DLL ConvertTo32( void );
    int  DLL ConvertTo24( void );
    int  DLL ConvertTo16( void );
    int  DLL Convert8To17( int transindex );
    int  DLL Convert32To17( void );
    int  SaveBmp(char *szFilename);
    int  LoadBmp(char *szFilename);
    int  SaveTga(char *szFilename, bool bCompressed );
    int  LoadTga(char *szFilename);
    int  DLL Load(char *szFilename);
    int  DLL Save(char *szFilename);

  private:
    void Output( char * pcData, int nSize );
    void Output( char c );
    void Output( void );
    unsigned char Input( void );

  public:
    int              m_Xres, m_Yres;
    unsigned short   m_BitPerPixel;
    unsigned short   m_BytePerPixel;
    unsigned char  * m_pBitmap;
    _BGR             m_Pal[256];

  private:
    int              m_NumPixel;
    FILE             * f;
    int              m_nCount;
    char             m_cBuf[32768];
};

#pragma pack()

}