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

#include <stdlib.h>
#include <string.h>
#include "hqnx_asm_Image.h"

#ifndef _MSC_VER
#define _stricmp strcasecmp
#endif

namespace HQnX_asm
{

DLL CImage::CImage() 
{ 
  m_Xres = m_Yres = m_NumPixel = 0; 
  m_pBitmap = NULL; 
}

DLL CImage::~CImage()
{
  Destroy();
}

int DLL CImage::Init( int X, int Y, unsigned short BitPerPixel )
{
  if (m_pBitmap != NULL)
    free(m_pBitmap);

  m_Xres = X;
  m_Yres = Y;
  m_BitPerPixel = BitPerPixel<=8 ? 8 : BitPerPixel<=16 ? 16 : BitPerPixel<=24 ? 24 : 32;
  m_BytePerPixel = m_BitPerPixel >> 3;
  m_NumPixel = m_Xres*m_Yres;
  int size = m_NumPixel*((m_BitPerPixel+7)/8);
  m_pBitmap=(unsigned char *)malloc(size);
  return (m_pBitmap != NULL) ? 0 : 1;
}

int DLL CImage::SetImage(unsigned char *img, int width, int height, int bpp)
{
   Init(width, height, bpp);

   memcpy(m_pBitmap, img, m_NumPixel * m_BytePerPixel);

   return 0;
}

int DLL CImage::Destroy()
{
   if (m_pBitmap)
   {
      free(m_pBitmap);
      m_pBitmap = NULL;
   }
   m_Xres = 0;
   m_Yres = 0;
   m_NumPixel = 0;
   m_BitPerPixel = 0;
   return 0;
}

int DLL CImage::Convert32To17( void )
{
  int nRes = eConvUnknownFormat;

  if ( m_BitPerPixel == 32 )
  {
    if ( m_pBitmap != NULL )
    {
      unsigned char * pTemp8 = m_pBitmap;
      unsigned int * pTemp32 = (unsigned int *)m_pBitmap;
      unsigned int a, r, g, b;
      for ( int i=0; i<m_NumPixel; i++ )
      {
        b = (*(pTemp8++)) >> 3;
        g = (*(pTemp8++)) >> 2;
        r = (*(pTemp8++)) >> 3;
        a = *(pTemp8++);
        *pTemp32 = (r << 11) + (g << 5) + b + (a > 127 ? 0x10000 : 0);
        pTemp32++;
      }
    }
    else
      nRes = eConvSourceMemory;

    nRes = 0;
  }

  return nRes;
}



}