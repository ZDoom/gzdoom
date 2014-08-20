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

int DLL CImage::ConvertTo32( void )
{
  int nRes = eConvUnknownFormat;

  if ( m_pBitmap == NULL )
    return eConvSourceMemory;

  switch ( m_BitPerPixel )
  {
    case 8:
    {
      nRes = 0;
      m_BitPerPixel = 32;
      unsigned char * pNewBitmap = (unsigned char *)malloc(m_NumPixel*4);
      if ( pNewBitmap != NULL )
      {
        unsigned char * pTemp8 = m_pBitmap;
        unsigned char * pTemp32 = pNewBitmap;
        unsigned char c;
        for ( int i=0; i<m_NumPixel; i++ )
        {
          c = *(pTemp8++);
          *(pTemp32++) = m_Pal[c].b;
          *(pTemp32++) = m_Pal[c].g;
          *(pTemp32++) = m_Pal[c].r;
          *(pTemp32++) = 0;
        }
        free(m_pBitmap);
        m_pBitmap = pNewBitmap;
      }
      else
        nRes = eConvDestMemory;

      break;
    }
    case 16:
    {
      nRes = 0;
      m_BitPerPixel = 32;
      unsigned char  * pNewBitmap = (unsigned char *)malloc(m_NumPixel*4);
      if ( pNewBitmap != NULL )
      {
        unsigned short * pTemp16 = (unsigned short *)m_pBitmap;
        unsigned int   * pTemp32 = (unsigned int *)pNewBitmap;
        unsigned short rgb;
        for ( int i=0; i<m_NumPixel; i++ )
        {
          rgb = *(pTemp16++);
          *(pTemp32++) = ((rgb & 0xF800) << 8) + ((rgb & 0x07E0) << 5) + ((rgb & 0x001F) << 3);
        }
        free(m_pBitmap);
        m_pBitmap = pNewBitmap;
      }
      else
        nRes = eConvDestMemory;

      break;
    }
    case 24:
    {
      nRes = 0;
      m_BitPerPixel = 32;
      unsigned char * pNewBitmap = (unsigned char *)malloc(m_NumPixel*4);
      if ( pNewBitmap != NULL )
      {
        unsigned char * pTemp24 = m_pBitmap;
        unsigned char * pTemp32 = pNewBitmap;
        for ( int i=0; i<m_NumPixel; i++ )
        {
          *(pTemp32++) = *(pTemp24++);
          *(pTemp32++) = *(pTemp24++);
          *(pTemp32++) = *(pTemp24++);
          *(pTemp32++) = 0;
        }
        free(m_pBitmap);
        m_pBitmap = pNewBitmap;
      }
      else
        nRes = eConvDestMemory;

      break;
    }
  }

  return nRes;
}

int CImage::ConvertTo24( void )
{
  int nRes = eConvUnknownFormat;

  if ( m_pBitmap == NULL )
    return eConvSourceMemory;

  switch ( m_BitPerPixel )
  {
    case 8:
    {
      nRes = 0;
      m_BitPerPixel = 24;
      unsigned char * pNewBitmap = (unsigned char *)malloc(m_NumPixel*3);
      if ( pNewBitmap != NULL )
      {
        unsigned char * pTemp8 = m_pBitmap;
        unsigned char * pTemp24 = pNewBitmap;
        unsigned char c;
        for ( int i=0; i<m_NumPixel; i++ )
        {
          c = *(pTemp8++);
          *(pTemp24++) = m_Pal[c].b;
          *(pTemp24++) = m_Pal[c].g;
          *(pTemp24++) = m_Pal[c].r;
        }
        free(m_pBitmap);
        m_pBitmap = pNewBitmap;
      }
      else
        nRes = eConvDestMemory;

      break;
    }
    case 16:
    {
      nRes = 0;
      m_BitPerPixel = 24;
      unsigned char  * pNewBitmap = (unsigned char *)malloc(m_NumPixel*3);
      if ( pNewBitmap != NULL )
      {
        unsigned short * pTemp16 = (unsigned short *)m_pBitmap;
        unsigned char  * pTemp24 = pNewBitmap;
        unsigned short rgb;
        for ( int i=0; i<m_NumPixel; i++ )
        {
          rgb = *(pTemp16++);
          *(pTemp24++) = ((rgb & 0x001F) << 3);
          *(pTemp24++) = ((rgb & 0x07E0) >> 3);
          *(pTemp24++) = ((rgb & 0xF800) >> 8);
        }
        free(m_pBitmap);
        m_pBitmap = pNewBitmap;
      }
      else
        nRes = eConvDestMemory;

      break;
    }
    case 32:
    {
      nRes = 0;
      m_BitPerPixel = 24;
      unsigned char * pNewBitmap = (unsigned char *)malloc(m_NumPixel*3);
      if ( pNewBitmap != NULL )
      {
        unsigned char * pTemp32 = m_pBitmap;
        unsigned char * pTemp24 = pNewBitmap;
        for ( int i=0; i<m_NumPixel; i++ )
        {
          *(pTemp24++) = *(pTemp32++);
          *(pTemp24++) = *(pTemp32++);
          *(pTemp24++) = *(pTemp32++);
          pTemp32++;
        }
        free(m_pBitmap);
        m_pBitmap = pNewBitmap;
      }
      else
        nRes = eConvDestMemory;

      break;
    }
  }

  return nRes;
}

int DLL CImage::ConvertTo16( void )
{
  int nRes = eConvUnknownFormat;

  if ( m_pBitmap == NULL )
    return eConvSourceMemory;

  switch ( m_BitPerPixel )
  {
    case 8:
    {
      nRes = 0;
      m_BitPerPixel = 16;
      unsigned char * pNewBitmap = (unsigned char *)malloc(m_NumPixel*2);
      if ( pNewBitmap != NULL )
      {
        unsigned char * pTemp8 = m_pBitmap;
        unsigned short * pTemp16 = (unsigned short *)pNewBitmap;
        unsigned short r, g, b;
        unsigned char c;
        for ( int i=0; i<m_NumPixel; i++ )
        {
          c = *(pTemp8++);
          r = m_Pal[c].r >> 3;
          g = m_Pal[c].g >> 2;
          b = m_Pal[c].b >> 3;
          *(pTemp16++) = (r << 11) + (g << 5) + b;
        }
        free(m_pBitmap);
        m_pBitmap = pNewBitmap;
      }
      else
        nRes = eConvDestMemory;

      break;
    }
    case 24:
    {
      nRes = 0;
      m_BitPerPixel = 16;
      unsigned char * pNewBitmap = (unsigned char *)malloc(m_NumPixel*2);
      if ( pNewBitmap != NULL )
      {
        unsigned char * pTemp24 = m_pBitmap;
        unsigned short * pTemp16 = (unsigned short *)pNewBitmap;
        unsigned short r, g, b;
        for ( int i=0; i<m_NumPixel; i++ )
        {
          b = (*(pTemp24++)) >> 3;
          g = (*(pTemp24++)) >> 2;
          r = (*(pTemp24++)) >> 3;
          *(pTemp16++) = (r << 11) + (g << 5) + b;
        }
        free(m_pBitmap);
        m_pBitmap = pNewBitmap;
      }
      else
        nRes = eConvDestMemory;

      break;
    }
    case 32:
    {
      nRes = 0;
      m_BitPerPixel = 16;
      unsigned char * pNewBitmap = (unsigned char *)malloc(m_NumPixel*2);
      if ( pNewBitmap != NULL )
      {
        unsigned char * pTemp32 = m_pBitmap;
        unsigned short * pTemp16 = (unsigned short *)pNewBitmap;
        unsigned short r, g, b;
        for ( int i=0; i<m_NumPixel; i++ )
        {
          b = (*(pTemp32++)) >> 3;
          g = (*(pTemp32++)) >> 2;
          r = (*(pTemp32++)) >> 3;
          pTemp32++;
          *(pTemp16++) = (r << 11) + (g << 5) + b;
        }
        free(m_pBitmap);
        m_pBitmap = pNewBitmap;
      }
      else
        nRes = eConvDestMemory;

      break;
    }
  }

  return nRes;
}

int CImage::Convert8To17( int transindex )
{
  int nRes = eConvUnknownFormat;

  if ( m_BitPerPixel == 8 )
  {
    m_BitPerPixel = 32;
    unsigned char * pNewBitmap = (unsigned char *)malloc(m_NumPixel*4);
    if ( pNewBitmap != NULL )
    {
      unsigned char * pTemp8 = m_pBitmap;
      unsigned int * pTemp32 = (unsigned int *)pNewBitmap;
      unsigned int r, g, b;
      unsigned char c;
      for ( int i=0; i<m_NumPixel; i++ )
      {
        c = *(pTemp8++);
        r = m_Pal[c].r >> 3;
        g = m_Pal[c].g >> 2;
        b = m_Pal[c].b >> 3;
        *(pTemp32++) = (r << 11) + (g << 5) + b + (transindex != c ? 0x10000 : 0);
      }
      free(m_pBitmap);
      m_pBitmap = pNewBitmap;
    }
    else
      nRes = eConvDestMemory;

    nRes = 0;
  }

  return nRes;
}

int CImage::SaveBmp(char *szFilename)
{
  _BMPFILEHEADER     fh;
  _BMPIMAGEHEADER    ih;
  unsigned char      BmpPal[256][4];
  long int           SuffLen;
  long int           Dummy = 0;
  unsigned char    * pBuf;
  short              i;

  if (!(f = fopen(szFilename, "wb"))) return eSaveBmpFileOpen;
  if ( m_pBitmap == NULL ) return eSaveBmpSourceMemory;

  fh.bfType=0x4D42;
  if (m_BitPerPixel==8)
  {
    SuffLen=((m_Xres+3)/4)*4-m_Xres;
    ih.biSize=0x28;
    ih.biWidth=m_Xres;
    ih.biHeight=m_Yres;
    ih.biPlanes=1;
    ih.biBitCount=8;
    ih.biCompression=0;
    ih.biSizeImage=(m_Xres+SuffLen)*m_Yres;
    ih.biXPelsPerMeter=ih.biYPelsPerMeter=0x2E23; // 300dpi (pixels per meter)
    ih.biClrUsed=ih.biClrImportant=0;
    fh.bfSize=(ih.biSizeImage)+0x0436;
    fh.bfRes1=0;
    fh.bfOffBits=0x0436;
    if (fwrite(&fh, 14, 1, f) != 1) return eSaveBmpFileWrite;
    if (fwrite(&ih, 40, 1, f) != 1) return eSaveBmpFileWrite;
    for (i=0; i<256; i++)
    {
      BmpPal[i][0]=m_Pal[i].b;
      BmpPal[i][1]=m_Pal[i].g;
      BmpPal[i][2]=m_Pal[i].r;
      BmpPal[i][3]=0;
    }
    if (fwrite(&BmpPal, 1024, 1, f) != 1) return eSaveBmpFileWrite;
    pBuf=m_pBitmap;
    pBuf+=m_NumPixel;
    for (i=0; i<m_Yres; i++)
    {
      pBuf-=m_Xres;
      if (fwrite(pBuf, m_Xres, 1, f) != 1) return eSaveBmpFileWrite;
      if (SuffLen>0) 
      { 
        if (fwrite(&Dummy, SuffLen, 1, f) != 1) return eSaveBmpFileWrite; 
      }
    }
  }
  else
  if (m_BitPerPixel==24)
  {
    SuffLen=((m_Xres*3+3)/4)*4-m_Xres*3;
    ih.biSize=0x28;
    ih.biWidth=m_Xres;
    ih.biHeight=m_Yres;
    ih.biPlanes=1;
    ih.biBitCount=24;
    ih.biCompression=0;
    ih.biSizeImage=(m_Xres*3+SuffLen)*m_Yres;
    ih.biXPelsPerMeter=ih.biYPelsPerMeter=0x2E23; // 300dpi (pixels per meter)
    ih.biClrUsed=ih.biClrImportant=0;
    fh.bfSize=(ih.biSizeImage)+0x0036;
    fh.bfRes1=0;
    fh.bfOffBits=0x0036;
    if (fwrite(&fh, 14, 1, f) != 1) return eSaveBmpFileWrite;
    if (fwrite(&ih, 40, 1, f) != 1) return eSaveBmpFileWrite;
    pBuf=m_pBitmap;
    pBuf+=m_NumPixel*3;
    for (i=0; i<m_Yres; i++)
    {
      pBuf-=m_Xres*3;
      if (fwrite(pBuf, m_Xres*3, 1, f) != 1) return eSaveBmpFileWrite;
      if (SuffLen>0) 
      { 
        if (fwrite(&Dummy, SuffLen, 1, f) != 1) return eSaveBmpFileWrite;
      }
    }
  }
  else
    return eSaveBmpColorDepth;

  fclose(f);

  return 0;
}

int CImage::LoadBmp(char *szFilename)
{
  _BMPFILEHEADER     fh;
  _BMPIMAGEHEADEROLD ih_old;
  _BMPIMAGEHEADER    ih;
  unsigned char      BmpPal[256][4];
  long int           biSize; 
  long int           SuffLen;
  long int           Dummy = 0;
  unsigned char    * pBuf;
  short              i;
  long int           xres, yres;
  unsigned short     bits;

  if (!(f = fopen(szFilename, "rb"))) return eLoadBmpFileOpen;
  if (fread(&fh, 14, 1, f) != 1) return eLoadBmpFileRead;
  if (fh.bfType != 0x4D42) return eLoadBmpBadFormat;
  if (fread(&biSize, 4, 1, f) != 1) return eLoadBmpFileRead;
  if (biSize > 12)
  {
    fseek( f, -4, SEEK_CUR );
    if (fread(&ih, biSize, 1, f) != 1) return eLoadBmpFileRead;
    xres = ih.biWidth;
    yres = ih.biHeight;
    bits = ih.biBitCount;
  }
  else
  {
    fseek( f, -4, SEEK_CUR );
    if (fread(&ih_old, biSize, 1, f) != 1) return eLoadBmpFileRead;
    xres = ih_old.biWidth;
    yres = ih_old.biHeight;
    bits = ih_old.biBitCount;
  }

  if ( Init( xres, yres, bits ) != 0 ) return eLoadBmpInit;
  if (m_BitPerPixel==8)
  {
    SuffLen=((m_Xres+3)/4)*4-m_Xres;
    if (fread(&BmpPal, 1024, 1, f) != 1) return eLoadBmpFileRead;
    for (i=0; i<256; i++)
    {
      m_Pal[i].b=BmpPal[i][0];
      m_Pal[i].g=BmpPal[i][1];
      m_Pal[i].r=BmpPal[i][2];
    }
    pBuf=m_pBitmap;
    pBuf+=m_NumPixel;
    for (i=0; i<m_Yres; i++)
    {
      pBuf-=m_Xres;
      if (fread(pBuf, m_Xres, 1, f) != 1) return eLoadBmpFileRead;
      if (SuffLen>0) 
      { 
        if (fread(&Dummy, SuffLen, 1, f) != 1) return eLoadBmpFileRead; 
      }
    }
  } 
  else
  if (m_BitPerPixel==24)
  {
    SuffLen=((m_Xres*3+3)/4)*4-(m_Xres*3);
    pBuf=m_pBitmap;
    pBuf+=m_NumPixel*3;
    for (i=0; i<m_Yres; i++)
    {
      pBuf-=m_Xres*3;
      if (fread(pBuf, m_Xres*3, 1, f) != 1) return eLoadBmpFileRead;
      if (SuffLen>0) 
      { 
        if (fread(&Dummy, SuffLen, 1, f) != 1) return eLoadBmpFileRead; 
      }
    }
  } 
  else 
    return eLoadBmpColorDepth;

  fclose(f);

  return 0;
}

void CImage::Output( void )
{
  fwrite(m_cBuf, m_nCount, 1, f);
  m_nCount=0;
}

void CImage::Output( char c )
{
  if ( m_nCount == sizeof(m_cBuf) )
  {
    fwrite(m_cBuf, m_nCount, 1, f);
    m_nCount=0;
  }
  m_cBuf[m_nCount++] = c;
}

void CImage::Output( char * pcData, int nSize )
{
  for ( int i=0; i<nSize; i++ )
    Output( *pcData++ );
}

unsigned char CImage::Input( void )
{
  if ( m_nCount == sizeof(m_cBuf) )
  {
    fread(m_cBuf, sizeof(m_cBuf), 1, f);
    m_nCount=0;
  }
  return m_cBuf[m_nCount++];
}

int CImage::SaveTga(char *szFilename, bool bCompressed )
{
  _TGAHEADER      fh = {0};
  int             i, j;
  unsigned char   nRep;
  unsigned char   nDif;
  bool            bEqual;

  if (!(f = fopen(szFilename, "wb"))) return eSaveTgaFileOpen;
  if ( m_pBitmap == NULL ) return eSaveTgaSourceMemory;

  fh.tiXres        = m_Xres;
  fh.tiYres        = m_Yres;
  fh.tiBitPerPixel = (unsigned char)m_BitPerPixel;

  if (m_BitPerPixel==8)
  {
    fh.tiPaletteIncluded = 1;
    fh.tiImageType    = bCompressed ? 9 : 1;
    fh.tiPaletteStart = 0;
    fh.tiPaletteSize  = 256;
    fh.tiPaletteBpp   = 24;
    if (fwrite(&fh, sizeof(fh), 1, f) != 1) return eSaveTgaFileWrite;

    if (fwrite(&m_Pal, 3, fh.tiPaletteSize, f) != fh.tiPaletteSize) 
      return eSaveTgaFileWrite;

    unsigned char * pcolBuf = m_pBitmap;
    pcolBuf += m_NumPixel;

    if ( !bCompressed )
    {
      for (j=0; j<m_Yres; j++)
      {
        pcolBuf -= m_Xres;
        if (fwrite(pcolBuf, 1, m_Xres, f) != (size_t)m_Xres) 
          return eSaveTgaFileWrite;
      }
    }
    else
    {
      unsigned char colOld;
      unsigned char colCur;
      m_nCount = 0;

      for (j=0; j<m_Yres; j++)
      {
        pcolBuf -= m_Xres;
        colCur = *pcolBuf;
        nRep = 0;
        nDif = 1;
        for (i=1; i<m_Xres; i++)
        {
          colOld = colCur;
          colCur = *(pcolBuf+i);
          bEqual = ( colOld == colCur );
          if ( nRep == 0 )
          {
            if ( (!bEqual) && (nDif<128) )
              nDif++;
            else
            {
              if ( bEqual ) 
                nRep = 1;
              nDif -= nRep;
              if ( nDif > 0 )
              {
                Output( nDif-1 );
                Output( (char*)(pcolBuf+i-nDif-nRep), nDif );
              }
              nDif = 1;
            }
          }
          else
          {
            if ( bEqual && (nRep<127) )
              nRep++;
            else
            {
              Output( nRep+128 );
              Output( (char*)&colOld, 1 );
              nRep = 0;
              nDif = 1;
            }
          }
        }

        if ( nRep == 0 )
        {
          Output( nDif-1 );
          Output( (char*)(pcolBuf+m_Xres-nDif), nDif );
        }
        else
        {
          Output( nRep+128 );
          Output( (char*)&colOld, 1 );
        }
        Output();
      }
    }
  }
  else
  if (m_BitPerPixel==24)
  {
    fh.tiImageType = bCompressed ? 10 : 2;
    if (fwrite(&fh, sizeof(fh), 1, f) != 1) return eSaveTgaFileWrite;

    _BGR * pcolBuf = (_BGR *)m_pBitmap;
    pcolBuf += m_NumPixel;

    if ( !bCompressed )
    {
      for (j=0; j<m_Yres; j++)
      {
        pcolBuf -= m_Xres;
        if (fwrite(pcolBuf, sizeof(_BGR), m_Xres, f) != (size_t)m_Xres) 
          return eSaveTgaFileWrite;
      }
    }
    else
    {
      _BGR colOld;
      _BGR colCur;
      m_nCount = 0;

      for (j=0; j<m_Yres; j++)
      {
        pcolBuf -= m_Xres;
        colCur = *pcolBuf;
        nRep = 0;
        nDif = 1;
        for (i=1; i<m_Xres; i++)
        {
          colOld = colCur;
          colCur = *(pcolBuf+i);
          bEqual = (colOld.r == colCur.r) && (colOld.g == colCur.g) && (colOld.b == colCur.b);
          if ( nRep == 0 )
          {
            if ( (!bEqual) && (nDif<128) )
              nDif++;
            else
            {
              if ( bEqual ) 
                nRep = 1;
              nDif -= nRep;
              if ( nDif > 0 )
              {
                Output( nDif-1 );
                Output( (char*)(pcolBuf+i-nDif-nRep), sizeof(_BGR)*nDif );
              }
              nDif = 1;
            }
          }
          else
          {
            if ( bEqual && (nRep<127) )
              nRep++;
            else
            {
              Output( nRep+128 );
              Output( (char*)&colOld, sizeof(_BGR) );
              nRep = 0;
              nDif = 1;
            }
          }
        }

        if ( nRep == 0 )
        {
          Output( nDif-1 );
          Output( (char*)(pcolBuf+m_Xres-nDif), sizeof(_BGR)*nDif );
        }
        else
        {
          Output( nRep+128 );
          Output( (char*)&colOld, sizeof(_BGR) );
        }
        Output();
      }
    }
  }
  else
  if (m_BitPerPixel==32)
  {
    fh.tiImageType = bCompressed ? 10 : 2;
    fh.tiAttrBits = 8;
    if (fwrite(&fh, sizeof(fh), 1, f) != 1) return eSaveTgaFileWrite;

    _BGRA * pcolBuf = (_BGRA *)m_pBitmap;
    pcolBuf += m_NumPixel;

    if ( !bCompressed )
    {
      for (j=0; j<m_Yres; j++)
      {
        pcolBuf -= m_Xres;
        if (fwrite(pcolBuf, sizeof(_BGRA), m_Xres, f) != (size_t)m_Xres) 
          return eSaveTgaFileWrite;
      }
    }
    else
    {
      _BGRA colOld;
      _BGRA colCur;
      m_nCount = 0;

      for (j=0; j<m_Yres; j++)
      {
        pcolBuf -= m_Xres;
        colCur = *pcolBuf;
        nRep = 0;
        nDif = 1;
        for (i=1; i<m_Xres; i++)
        {
          colOld = colCur;
          colCur = *(pcolBuf+i);
          bEqual = (colOld.r == colCur.r) && 
                   (colOld.g == colCur.g) && 
                   (colOld.b == colCur.b) &&
                   (colOld.a == colCur.a);
          if ( nRep == 0 )
          {
            if ( (!bEqual) && (nDif<128) )
              nDif++;
            else
            {
              if ( bEqual ) 
                nRep = 1;
              nDif -= nRep;
              if ( nDif > 0 )
              {
                Output( nDif-1 );
                Output( (char*)(pcolBuf+i-nDif-nRep), sizeof(_BGRA)*nDif );
              }
              nDif = 1;
            }
          }
          else
          {
            if ( bEqual && (nRep<127) )
              nRep++;
            else
            {
              Output( nRep+128 );
              Output( (char*)&colOld, sizeof(_BGRA) );
              nRep = 0;
              nDif = 1;
            }
          }
        }

        if ( nRep == 0 )
        {
          Output( nDif-1 );
          Output( (char*)(pcolBuf+m_Xres-nDif), sizeof(_BGRA)*nDif );
        }
        else
        {
          Output( nRep+128 );
          Output( (char*)&colOld, sizeof(_BGRA) );
        }
        Output();
      }
    }
  }
  else
    return eSaveTgaColorDepth;

  fclose(f);

  return 0;
}

int CImage::LoadTga(char *szFilename)
{
  _TGAHEADER      fh;
  int             i, j, k;
  unsigned char   nCount;

  if (!(f = fopen(szFilename, "rb"))) return eLoadTgaFileOpen;
  if (fread(&fh, sizeof(fh), 1, f) != 1) return eLoadTgaFileRead;
  bool bCompressed = (( fh.tiImageType & 8 ) != 0);
  if ((fh.tiBitPerPixel<=0) || (fh.tiBitPerPixel>32))
    return eLoadTgaBadFormat;

  if ( Init( fh.tiXres, fh.tiYres, fh.tiBitPerPixel ) != 0 ) 
    return eLoadTgaInit;

  if ( m_BitPerPixel == 8 )
  {
    if ( fh.tiPaletteIncluded == 1 )
    {
      if ( fh.tiPaletteBpp == 24)
      {
        if (fread(&m_Pal, 3, fh.tiPaletteSize, f) != fh.tiPaletteSize) 
          return eLoadTgaFileRead;
      }
      else
      if ( fh.tiPaletteBpp == 32)
      {
        unsigned char BmpPal[256][4];

        if (fread(&BmpPal, 4, fh.tiPaletteSize, f) != fh.tiPaletteSize) 
          return eLoadTgaFileRead;

        for (i=0; i<fh.tiPaletteSize; i++)
        {
          m_Pal[i].b = BmpPal[i][0];
          m_Pal[i].g = BmpPal[i][1];
          m_Pal[i].r = BmpPal[i][2];
        }
      }
    }
    else
    {
      for (i=0; i<256; i++)
      {
        m_Pal[i].r = i;
        m_Pal[i].g = i;
        m_Pal[i].b = i;
      }
    }

    unsigned char * pcolBuf = m_pBitmap;
    pcolBuf += m_NumPixel;

    if ( !bCompressed )
    {
      for (j=0; j<m_Yres; j++)
      {
        pcolBuf -= m_Xres;
        if (fread(pcolBuf, 1, m_Xres, f) != (size_t)m_Xres) 
          return eLoadTgaFileRead;
      }
    }
    else
    {
      unsigned char colCur;

      pcolBuf -= m_Xres;

      i = 0;
      m_nCount = sizeof(m_cBuf);

      while ( pcolBuf >= m_pBitmap )
      {
        nCount = Input();
        if ((nCount & 128)==0)
        {
          for (k=0; k<=nCount; k++)
          {
            colCur = Input();
            *(pcolBuf+i) = colCur;
            if ( (++i) == m_Xres )
            {
              i=0;
              pcolBuf -= m_Xres;
              break;
            }
          }
        }
        else
        {
          colCur = Input();
          for (k=0; k<=nCount-128; k++)
          {
            *(pcolBuf+i) = colCur;
            if ( (++i) == m_Xres )
            {
              i=0;
              pcolBuf -= m_Xres;
              break;
            }
          }
        }
      }
    }
  }
  else
  if ( m_BitPerPixel == 24 )
  {
    _BGR * pcolBuf = (_BGR *)m_pBitmap;
    pcolBuf += m_NumPixel;

    if ( !bCompressed )
    {
      for (j=0; j<m_Yres; j++)
      {
        pcolBuf -= m_Xres;
        if (fread(pcolBuf, sizeof(_BGR), m_Xres, f) != (size_t)m_Xres) 
          return eLoadTgaFileRead;
      }
    }
    else
    {
      _BGR colCur;

      pcolBuf -= m_Xres;

      i = 0;
      m_nCount = sizeof(m_cBuf);

      while ( pcolBuf >= (_BGR *)m_pBitmap )
      {
        nCount = Input();
        if ((nCount & 128)==0)
        {
          for (k=0; k<=nCount; k++)
          {
            colCur.b = Input();
            colCur.g = Input();
            colCur.r = Input();
            *(pcolBuf+i) = colCur;
            if ( (++i) == m_Xres )
            {
              i=0;
              pcolBuf -= m_Xres;
              break;
            }
          }
        }
        else
        {
          colCur.b = Input();
          colCur.g = Input();
          colCur.r = Input();
          for (k=0; k<=nCount-128; k++)
          {
            *(pcolBuf+i) = colCur;
            if ( (++i) == m_Xres )
            {
              i=0;
              pcolBuf -= m_Xres;
              break;
            }
          }
        }
      }
    }
  }
  else
  if ( m_BitPerPixel == 32 )
  {
    _BGRA * pcolBuf = (_BGRA *)m_pBitmap;
    pcolBuf += m_NumPixel;

    if ( !bCompressed )
    {
      for (j=0; j<m_Yres; j++)
      {
        pcolBuf -= m_Xres;
        if (fread(pcolBuf, sizeof(_BGRA), m_Xres, f) != (size_t)m_Xres) 
          return eLoadTgaFileRead;
      }
    }
    else
    {
      _BGRA colCur;

      pcolBuf -= m_Xres;

      i = 0;
      m_nCount = sizeof(m_cBuf);

      while ( pcolBuf >= (_BGRA *)m_pBitmap )
      {
        nCount = Input();
        if ((nCount & 128)==0)
        {
          for (k=0; k<=nCount; k++)
          {
            colCur.b = Input();
            colCur.g = Input();
            colCur.r = Input();
            colCur.a = Input();
            *(pcolBuf+i) = colCur;
            if ( (++i) == m_Xres )
            {
              i=0;
              pcolBuf -= m_Xres;
              break;
            }
          }
        }
        else
        {
          colCur.b = Input();
          colCur.g = Input();
          colCur.r = Input();
          colCur.a = Input();
          for (k=0; k<=nCount-128; k++)
          {
            *(pcolBuf+i) = colCur;
            if ( (++i) == m_Xres )
            {
              i=0;
              pcolBuf -= m_Xres;
              break;
            }
          }
        }
      }
    }
  }
  else 
    return eLoadTgaColorDepth;

  fclose(f);

  return 0;
}

int DLL CImage::Load(char *szFilename)
{
  int nRes = 0;

  if ( szFilename != NULL ) 
  {
    char * szExt = strrchr( szFilename, '.' );
    int nNotTGA = 1;

    if ( szExt != NULL ) 
      nNotTGA = _stricmp( szExt, ".tga" );

    if ( nNotTGA != 0 )
      nRes = LoadBmp( szFilename );
    else
      nRes = LoadTga( szFilename );
  }
  else
    nRes = eLoadFilename;

  return nRes;
}

int DLL CImage::Save(char *szFilename)
{
  int nRes = 0;
  int nNotTGA = 1;

  if ( szFilename != NULL ) 
  {
    char * szExt = strrchr( szFilename, '.' );

    if ( szExt != NULL ) 
      nNotTGA = _stricmp( szExt, ".tga" );

    if ( nNotTGA != 0 )
    {
      if (( m_BitPerPixel == 16 ) || ( m_BitPerPixel == 32 ))
        nRes = ConvertTo24();

      if (nRes == 0)
        nRes = SaveBmp( szFilename );
    }
    else
    {
      if ( m_BitPerPixel == 16 )
        nRes = ConvertTo24();

      if (nRes == 0)
        nRes = SaveTga( szFilename, true );
    }
  }
  else
    nRes = eSaveFilename;

  return nRes;
}

}