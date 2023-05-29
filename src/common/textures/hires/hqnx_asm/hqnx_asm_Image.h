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
#pragma once
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

  };


  public:
    int  DLL Init( int Xres, int Yres, unsigned short BitPerPixel );
    int  DLL SetImage(unsigned char *img, int width, int height, int bpp);
    int  DLL Destroy();
    int  DLL Convert32To17( void );

  private:

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