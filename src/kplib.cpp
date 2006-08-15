/**************************************************************************************************
KPLIB.C: Ken's Picture LIBrary written by Ken Silverman
Copyright (c) 1998-2005 Ken Silverman
Ken Silverman's official web site: http://www.advsys.net/ken

This source file includes routines for decompression of the following picture formats:
	JPG,PNG,GIF,PCX,TGA,BMP,CEL
It also includes code to handle ZIP decompression.

Brief history:
1998?: Wrote KPEG, a JPEG viewer for DOS
2000: Wrote KPNG, a PNG viewer for DOS
2001: Combined KPEG&KPNG, ported to Visual C, and made it into a nice library called KPLIB.C
2002: Added support for: TGA,GIF,CEL,ZIP
2003: Added support for: BMP
05/18/2004: Added support for 8/24 bit PCX

I offer this code to the community for the benefit of Jonathon Fowler's Duke3D port.

-Ken S.
**************************************************************************************************/
// [RH] Removed everything but JPEG support.

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "m_swap.h"
#include "m_fixed.h"

#if !defined(_WIN32) && !defined(__DOS__)
#include <unistd.h>
static __inline unsigned long _lrotl (unsigned long i, int sh)
	{ return((i>>(-sh))|(i<<sh)); }
#define _fileno fileno
#else
#include <io.h>
#endif

#if !defined(max)
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#if !defined(min)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#if defined(__GNUC__)
#define _inline inline
#endif

	//use GCC-specific extension to force symbol name to be something in particular to override underscoring.

static int bytesperline, xres, yres, globxoffs, globyoffs;
static unsigned char *frameplace;

static const int pow2mask[32] =
{
	0x00000000,0x00000001,0x00000003,0x00000007,
	0x0000000f,0x0000001f,0x0000003f,0x0000007f,
	0x000000ff,0x000001ff,0x000003ff,0x000007ff,
	0x00000fff,0x00001fff,0x00003fff,0x00007fff,
	0x0000ffff,0x0001ffff,0x0003ffff,0x0007ffff,
	0x000fffff,0x001fffff,0x003fffff,0x007fffff,
	0x00ffffff,0x01ffffff,0x03ffffff,0x07ffffff,
	0x0fffffff,0x1fffffff,0x3fffffff,0x7fffffff,
};

//Initialized tables (can't be in union)
//jpg:                png:
//   crmul      16384    abstab10    4096
//   cbmul      16384    hxbit        472
//   dct         4608    pow2mask     128*
//   colclip     4096
//   colclipup8  4096
//   colclipup16 4096
//   unzig        256
//   pow2mask     128*
//   dcflagor      64

int palcol[256], paleng;
unsigned char coltype, bitdepth;

//============================ KPEGILIB begins ===============================

	//11/01/2000: This code was originally from KPEG.C
	//   All non 32-bit color drawing was removed
	//   "Motion" JPG code was removed
	//   A lot of parameters were added to kpeg() for library usage
static int clipxdim, clipydim;

static int hufquickcnt[8];
static int hufmaxatbit[8][20], hufvalatbit[8][20], hufcnt[8];
static unsigned char hufnumatbit[8][20], huftable[8][256];
static unsigned char gnumcomponents, dcflagor[64];
static int gcompid[4], gcomphsamp[4], gcompvsamp[4], gcompquantab[4];
static int lcompid[4], lcompdc[4], lcompac[4];
static int lcomphsamp[4], lcompvsamp[4], lcomphvsamp[4], lcompquantab[4];
static int lcomphsampmask[4], lcomphsampshift[4], lcompvsampshift[4];
static int quantab[4][64], dct[19][64], lastdc[4], unzig[64];
static const unsigned char pow2char[8] = {1,2,4,8,16,32,64,128};

#define SQRT2 23726566   //(sqrt(2))<<24
#define C182 31000253    //(cos(PI/8)*2)<<24
#define C18S22 43840978  //(cos(PI/8)*sqrt(2)*2)<<24
#define C38S22 18159528  //(cos(PI*3/8)*sqrt(2)*2)<<24
static const int cosqr16[8] =    //cosqr16[i] = ((cos(PI*i/16)*sqrt(2))<<24);
  {23726566,23270667,21920489,19727919,16777216,13181774,9079764,4628823};

// [RH] Moved the bigger tables into a dynamically allocated struct so that
// they don't waste space if a JPEG is never loaded.
struct kpegtables
{
	int hufquickval[8][1024], hufquickbits[8][1024];
	int colclip[1024], colclipup8[1024], colclipup16[1024];
	int crmul[4096], cbmul[4096];
};
static kpegtables *kpeg;

static void initkpeg ()
{
	int i, x, y;

	kpeg = new kpegtables;

	x = 0;  //Back & forth diagonal pattern (aligning bytes for best compression)
	for(i=0;i<16;i+=2)
	{
		for(y=7;y>=0;y--)
			if ((unsigned)(i-y) < (unsigned)8) unzig[x++] = (y<<3)+i-y;
		for(y=0;y<8;y++)
			if ((unsigned)(i+1-y) < (unsigned)8) unzig[x++] = (y<<3)+i+1-y;
	}
	for(i=63;i>=0;i--) dcflagor[i] = (unsigned char)(1<<(unzig[i]>>3));

	for(i=0;i<128;i++) kpeg->colclip[i] = i+128;
	for(i=128;i<512;i++) kpeg->colclip[i] = 255;
	for(i=512;i<896;i++) kpeg->colclip[i] = 0;
	for(i=896;i<1024;i++) kpeg->colclip[i] = i-896;
	for(i=0;i<1024;i++)
	{
		kpeg->colclipup8[i] = (kpeg->colclip[i]<<8);
		kpeg->colclipup16[i] = (kpeg->colclip[i]<<16)+0xff000000; //Hack: set alphas to 255
	}

	for(i=0;i<2048;i++)
	{
		kpeg->crmul[(i<<1)+0] = (i-1024)*1470104; //1.402*1048576
		kpeg->crmul[(i<<1)+1] = (i-1024)*-748830; //-0.71414*1048576
		kpeg->cbmul[(i<<1)+0] = (i-1024)*-360857; //-0.34414*1048576
		kpeg->cbmul[(i<<1)+1] = (i-1024)*1858077; //1.772*1048576
	}

	memset((void *)&dct[16][0],0,64*2*sizeof(dct[0][0]));
	memset((void *)&dct[18][0],0xa0,64*sizeof(dct[0][0]));
}

static void huffgetval (int index, int curbits, int num, int *daval, int *dabits)
{
	int b, v, pow2, *hmax;

	hmax = &hufmaxatbit[index][0];
	pow2 = pow2mask[curbits-1]+1;
	if (num&pow2) v = 1; else v = 0;
	for(b=1;b<=16;b++)
	{
		if (v < hmax[b])
		{
			*dabits = b;
			*daval = huftable[index][hufvalatbit[index][b]+v];
			return;
		}
		pow2 >>= 1; v <<= 1;
		if (num&pow2) v++;
	}
	*dabits = 16;
	*daval = 0;
}

int kpegrend (const char *kfilebuf, int kfilength,
	unsigned char *daframeplace, int dabytesperline, int daxres, int dayres,
	int daglobxoffs, int daglobyoffs)
{
	int i, j, v, leng, xdim = 0, ydim = 0, index, prec, restartinterval;
	int restartcnt, num, curbits, x, y, z, dctcnt, c, cc, daval, dabits;
	int xx, yy, zz, xxx, yyy, r, g, b, t0, t1, t2, t3, t4, t5, t6, t7;
	int yv, cr = 0, cb = 0, *dc, *dc2, xxxend, yyyend;
	int *hqval, *hqbits, hqcnt, *quanptr;
	unsigned char ch, marker, numbits, lnumcomponents, dcflag, *p;
	const unsigned char *kfileptr;

	if (kpeg == NULL) { initkpeg(); }

	kfileptr = (unsigned char *)kfilebuf;

	if (*(unsigned short *)kfileptr == 0xd8ff) kfileptr += 2;
	else return(-1); //"%s is not a JPEG file\n",filename

	restartinterval = 0;
	for(i=0;i<4;i++) lastdc[i] = 0;
	for(i=0;i<8;i++) hufcnt[i] = 0;

	coltype = 0; bitdepth = 8; //For PNGOUT
	do
	{
		ch = *kfileptr++; if (ch != 255) continue;
		marker = *kfileptr++;
		leng = ((int)kfileptr[0]<<8)+(int)kfileptr[1]-2;
		kfileptr += 2;
		switch(marker)
		{
			case 0xc0: case 0xc1: case 0xc2:
					//processit!
				numbits = *kfileptr++;

				ydim = ((int)kfileptr[0]<<8)+(int)kfileptr[1];
				xdim = ((int)kfileptr[2]<<8)+(int)kfileptr[3];
				//printf("%s: %ld / %ld = %ld\n",filename,xdim*ydim*3,kfilength,(xdim*ydim*3)/kfilength);

				frameplace = daframeplace;
				bytesperline = dabytesperline;
				xres = daxres;
				yres = dayres;
				globxoffs = daglobxoffs;
				globyoffs = daglobyoffs;

				gnumcomponents = kfileptr[4];
				kfileptr += 5;
				for(z=0;z<gnumcomponents;z++)
				{
					gcompid[z] = kfileptr[0];
					gcomphsamp[z] = (kfileptr[1]>>4);
					gcompvsamp[z] = (kfileptr[1]&15);
					gcompquantab[z] = kfileptr[2];
					kfileptr += 3;
				}
				break;
			case 0xc4:  //Huffman table
				do
				{
					ch = *kfileptr++; leng--;
					if (ch >= 16) { index = ch-12; }
								else { index = ch; }
					memcpy((void *)&hufnumatbit[index][1],(void *)kfileptr,16); kfileptr += 16;
					leng -= 16;

					v = 0; hufcnt[index] = 0;
					hufquickcnt[index] = 0; c = 0;
					for(b=1;b<=16;b++)
					{
						hufmaxatbit[index][b] = v+hufnumatbit[index][b];
						hufvalatbit[index][b] = hufcnt[index]-v;
						memcpy((void *)&huftable[index][hufcnt[index]],(void *)kfileptr,(int)hufnumatbit[index][b]);
						if (b <= 10)
							for(c=0;c<hufnumatbit[index][b];c++)
								for(j=(1<<(10-b));j>0;j--)
								{
									kpeg->hufquickval[index][hufquickcnt[index]] = huftable[index][hufcnt[index]+c];
									kpeg->hufquickbits[index][hufquickcnt[index]] = b;
									hufquickcnt[index]++;
								}
						kfileptr += hufnumatbit[index][b];
						leng -= hufnumatbit[index][b];
						hufcnt[index] += hufnumatbit[index][b];
						v = ((v+hufnumatbit[index][b])<<1);
					}

				} while (leng > 0);
				break;
			case 0xdb:
				do
				{
					ch = *kfileptr++; leng--;
					index = (ch&15);
					prec = (ch>>4);
					for(z=0;z<64;z++)
					{
						v = (int)(*kfileptr++);
						if (prec) v = (v<<8)+((int)(*kfileptr++));
						v <<= 19;
						if (unzig[z]&7) v = MulScale24(v,cosqr16[unzig[z]&7]);
						if (unzig[z]>>3) v = MulScale24(v,cosqr16[unzig[z]>>3]);
						if (index) v >>= 6;
						quantab[index][unzig[z]] = v;
					}
					leng -= 64;
					if (prec) leng -= 64;
				} while (leng > 0);
				break;
			case 0xdd:
				restartinterval = (((int)kfileptr[0])<<8)+((int)kfileptr[1]);
				kfileptr += leng;
				break;
			case 0xda: case 0xd9:
				if ((xdim <= 0) || (ydim <= 0)) return(-1);

				lnumcomponents = *kfileptr++;
				if (lnumcomponents > 1) coltype = 2;
				for(z=0;z<lnumcomponents;z++)
				{
					lcompid[z] = kfileptr[0];
					lcompdc[z] = (kfileptr[1]>>4);
					lcompac[z] = (kfileptr[1]&15);
					kfileptr += 2;
					for(zz=0;zz<gnumcomponents;zz++)
						if (lcompid[z] == gcompid[zz])
						{
							lcomphsamp[z] = gcomphsamp[zz];
							lcompvsamp[z] = gcompvsamp[zz];
							lcomphvsamp[z] = lcomphsamp[z]*lcompvsamp[z];
							lcompquantab[z] = gcompquantab[zz];

							for(i=0;i<8;i++)
								if (pow2mask[i]+1 == lcomphsamp[z])
								{
									lcomphsampmask[z] = pow2mask[i];
									lcomphsampshift[z] = i;
									break;
								}

							for(i=0;i<8;i++)
								if (pow2mask[i]+1 == lcompvsamp[z])
								{
									lcompvsampshift[z] = i;
									break;
								}
						}
				}
				//Ss = kfileptr[0];
				//Se = kfileptr[1];
				//Ah = (kfileptr[2]>>4);
				//Al = (kfileptr[2]&15);
				kfileptr += 3;

				if ((hufcnt[0] == 0) || (hufcnt[4] == 0)) return(-1);

				clipxdim = min(xdim+globxoffs,xres);
				clipydim = min(ydim+globyoffs,yres);

				xx = max(globxoffs,0); xxx = min(globxoffs+xdim,xres);
				yy = max(globyoffs,0); yyy = min(globyoffs+ydim,yres);
				if ((xx >= xres) || (yy >= yres) || (xxx <= 0) || (yyy <= 0)) return(0);

				restartcnt = restartinterval; marker = 0xd0;
				num = 0; curbits = 0; x = 0; y = 0;
				while (1)
				{
					if (kfileptr-(unsigned char *)kfilebuf >= kfilength)
						lnumcomponents = 0; //rest of file is missing!

					dctcnt = 0;
					for(c=0;c<lnumcomponents;c++)
					{
						hqval = &kpeg->hufquickval[lcompac[c]+4][0];
						hqbits = &kpeg->hufquickbits[lcompac[c]+4][0];
						hqcnt = hufquickcnt[lcompac[c]+4];
						quanptr = &quantab[lcompquantab[c]][0];
						for(cc=lcomphvsamp[c];cc>0;cc--)
						{
							dc = &dct[dctcnt][0];

								//Get DC
							while (curbits < 16) //Getbits
							{
								ch = *kfileptr++; if (ch == 255) kfileptr++;
								num = (num<<8)+((int)ch); curbits += 8;
							}

							i = ((num>>(curbits-10))&1023);
							if (i < hufquickcnt[lcompdc[c]])
								{ dabits = kpeg->hufquickbits[lcompdc[c]][i]; daval = kpeg->hufquickval[lcompdc[c]][i]; }
							else
								huffgetval(lcompdc[c],curbits,num,&daval,&dabits);

							curbits -= dabits;
							if (daval)
							{
								while (curbits < 16) //Getbits
								{
									ch = *kfileptr++; if (ch == 255) kfileptr++;
									num = (num<<8)+((int)ch); curbits += 8;
								}

								v = ((unsigned)num >> (curbits-daval)) & pow2mask[daval];
								if (v <= pow2mask[daval-1]) v -= pow2mask[daval];
								lastdc[c] += v;
								curbits -= daval;
							}
							dc[0] = lastdc[c]*quanptr[0];

								//Get AC
							memset((void *)&dc[1],0,63*4);
							dcflag = 1;
							for(z=1;z<64;z++)
							{
								while (curbits < 16) //Getbits
								{
									ch = *kfileptr++; if (ch == 255) kfileptr++;
									num = (num<<8)+((int)ch); curbits += 8;
								}

								i = ((num>>(curbits-10))&1023);
								if (i < hqcnt)
									{ daval = hqval[i]; curbits -= hqbits[i]; }
								else
								{
									huffgetval(lcompac[c]+4,curbits,num,&daval,&dabits);
									curbits -= dabits;
								}

								if (!daval) break;
								z += (daval>>4); if (z >= 64) break;
								daval &= 15;

								while (curbits < 16) //Getbits
								{
									ch = *kfileptr++; if (ch == 255) kfileptr++;
									num = (num<<8)+((int)ch); curbits += 8;
								}

								v = ((unsigned)num >> (curbits-daval)) & pow2mask[daval];
								if (v <= pow2mask[daval-1]) v -= pow2mask[daval];
								dcflag |= dcflagor[z];
								dc[unzig[z]] = v*quanptr[unzig[z]];
								curbits -= daval;
							}

							for(z=0;z<8;z++,dc+=8)
							{
								if (!(dcflag&pow2char[z])) continue;
								t3 = dc[2] + dc[6];
								t2 = (MulScale32(dc[2]-dc[6],SQRT2<<6)<<2) - t3;
								t4 = dc[0] + dc[4]; t5 = dc[0] - dc[4];
								t0 = t4+t3; t3 = t4-t3; t1 = t5+t2; t2 = t5-t2;
								t4 = (MulScale32(dc[5]-dc[3]+dc[1]-dc[7],C182<<6)<<2);
								t7 = dc[1] + dc[7] + dc[5] + dc[3];
								t6 = (MulScale32(dc[3]-dc[5],C18S22<<5)<<3) + t4 - t7;
								t5 = (MulScale32(dc[1]+dc[7]-dc[5]-dc[3],SQRT2<<6)<<2) - t6;
								t4 = (MulScale32(dc[1]-dc[7],C38S22<<6)<<2) - t4 + t5;
								dc[0] = t0+t7; dc[7] = t0-t7; dc[1] = t1+t6; dc[6] = t1-t6;
								dc[2] = t2+t5; dc[5] = t2-t5; dc[4] = t3+t4; dc[3] = t3-t4;
							}
							dc = &dct[dctcnt][0];
							for(z=7;z>=0;z--,dc++)
							{
								t3 = dc[2<<3] + dc[6<<3];
								t2 = (MulScale32(dc[2<<3]-dc[6<<3],SQRT2<<6)<<2) - t3;
								t4 = dc[0<<3] + dc[4<<3]; t5 = dc[0<<3] - dc[4<<3];
								t0 = t4+t3; t3 = t4-t3; t1 = t5+t2; t2 = t5-t2;
								t4 = (MulScale32(dc[5<<3]-dc[3<<3]+dc[1<<3]-dc[7<<3],C182<<6)<<2);
								t7 = dc[1<<3] + dc[7<<3] + dc[5<<3] + dc[3<<3];
								t6 = (MulScale32(dc[3<<3]-dc[5<<3],C18S22<<5)<<3) + t4 - t7;
								t5 = (MulScale32(dc[1<<3]+dc[7<<3]-dc[5<<3]-dc[3<<3],SQRT2<<6)<<2) - t6;
								t4 = (MulScale32(dc[1<<3]-dc[7<<3],C38S22<<6)<<2) - t4 + t5;
								dc[0<<3] = t0+t7; dc[7<<3] = t0-t7; dc[1<<3] = t1+t6; dc[6<<3] = t1-t6;
								dc[2<<3] = t2+t5; dc[5<<3] = t2-t5; dc[4<<3] = t3+t4; dc[3<<3] = t3-t4;
							}

							dctcnt++;
						}
					}

					dctcnt = 0; dc = &dct[18][0]; dc2 = &dct[16][0];
					r = g = b = 0; cc = 0;
					for(yy=0;yy<(lcompvsamp[0]<<3);yy+=8)
						for(xx=0;xx<(lcomphsamp[0]<<3);xx+=8,dctcnt++)
						{
							yyy = y+yy+globyoffs; if ((unsigned)yyy >= (unsigned)clipydim) continue;
							xxx = x+xx+globxoffs; if ((unsigned)xxx >= (unsigned)clipxdim) continue;
							p = yyy*bytesperline + xxx*4 + frameplace;
							if (lnumcomponents > 0) dc = &dct[dctcnt][0];
							if (lnumcomponents > 1) dc2 = &dct[lcomphvsamp[0]][((yy>>lcompvsampshift[0])<<3)+(xx>>lcomphsampshift[0])];
							xxxend = min(clipxdim-(x+xx+globxoffs),8);
							yyyend = min(clipydim-(y+yy+globyoffs),8);
							if ((lcomphsamp[0] == 1) && (xxxend == 8))
							{
								for(yyy=0;yyy<yyyend;yyy++)
								{
									for(xxx=0;xxx<8;xxx++)
									{
										yv = dc[xxx];
										cr = (dc2[xxx+64]>>13)&~1;
										cb = (dc2[xxx]>>13)&~1;
										((int *)p)[xxx] = kpeg->colclipup16[(unsigned)(yv+kpeg->crmul[cr+2048])>>22]+
																  kpeg->colclipup8[(unsigned)(yv+kpeg->crmul[cr+2049]+kpeg->cbmul[cb+2048])>>22]+
																	  kpeg->colclip[(unsigned)(yv+kpeg->cbmul[cb+2049])>>22];
									}
									p += bytesperline;
									dc += 8;
									if (!((yyy+1)&(lcompvsamp[0]-1))) dc2 += 8;
								}
							}
							else if ((lcomphsamp[0] == 2) && (xxxend == 8))
							{
								for(yyy=0;yyy<yyyend;yyy++)
								{
									for(xxx=0;xxx<8;xxx+=2)
									{
										yv = dc[xxx];
										cr = (dc2[(xxx>>1)+64]>>13)&~1;
										cb = (dc2[(xxx>>1)]>>13)&~1;
										i = kpeg->crmul[cr+2049]+kpeg->cbmul[cb+2048];
										cr = kpeg->crmul[cr+2048];
										cb = kpeg->cbmul[cb+2049];
										((int *)p)[xxx] = kpeg->colclipup16[(unsigned)(yv+cr)>>22]+
																  kpeg->colclipup8[(unsigned)(yv+i)>>22]+
																	  kpeg->colclip[(unsigned)(yv+cb)>>22];
										yv = dc[xxx+1];
										((int *)p)[xxx+1] = kpeg->colclipup16[(unsigned)(yv+cr)>>22]+
																	 kpeg->colclipup8[(unsigned)(yv+i)>>22]+
																		 kpeg->colclip[(unsigned)(yv+cb)>>22];
									}
									p += bytesperline;
									dc += 8;
									if (!((yyy+1)&(lcompvsamp[0]-1))) dc2 += 8;
								}
							}
							else
							{
								for(yyy=0;yyy<yyyend;yyy++)
								{
									i = 0; j = 1;
									for(xxx=0;xxx<xxxend;xxx++)
									{
										yv = dc[xxx];
										j--;
										if (!j)
										{
											j = lcomphsamp[0];
											cr = (dc2[i+64]>>13)&~1;
											cb = (dc2[i]>>13)&~1;
											i++;
										}
										((int *)p)[xxx] = kpeg->colclipup16[(unsigned)(yv+kpeg->crmul[cr+2048])>>22]+
																  kpeg->colclipup8[(unsigned)(yv+kpeg->crmul[cr+2049]+kpeg->cbmul[cb+2048])>>22]+
																	  kpeg->colclip[(unsigned)(yv+kpeg->cbmul[cb+2049])>>22];
									}
									p += bytesperline;
									dc += 8;
									if (!((yyy+1)&(lcompvsamp[0]-1))) dc2 += 8;
								}
							}
						}

					if (lnumcomponents) //do only when not EOF...
					{
						restartcnt--;
						if (!restartcnt)
						{
							kfileptr += 1-(curbits>>3); curbits = 0;
							if ((kfileptr[-2] != 255) || (kfileptr[-1] != marker)) kfileptr--;
							marker++; if (marker >= 0xd8) marker = 0xd0;
							restartcnt = restartinterval;
							for(i=0;i<4;i++) lastdc[i] = 0;
						}
					}

					x += (lcomphsamp[0]<<3);
					if (x >= xdim) { x = 0; y += (lcompvsamp[0]<<3); if (y >= ydim) return(0); }
				}
			default:
				kfileptr += leng;
				break;
		}
	} while (kfileptr-(unsigned char *)kfilebuf < kfilength);
	return(0);
}

//==============================  KPEGILIB ends ==============================
