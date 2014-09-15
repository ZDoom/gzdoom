//hqnx filter library
//----------------------------------------------------------
//Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
//Copyright (C) 2009 Benjamin Berkels
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

#ifndef __HQNX_H__
#define __HQNX_H__

#pragma warning(disable:4799)

#include "hqnx_asm_Image.h"

namespace HQnX_asm
{
void DLL hq2x_32( int * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
void DLL hq3x_32( int * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
void DLL hq4x_32( int * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
int DLL hq4x_32 ( CImage &ImageIn, CImage &ImageOut );

void DLL InitLUTs();

}


#endif //__HQNX_H__