/*
** md5check.cpp
** Checksums for textures that need special treatment
**
**---------------------------------------------------------------------------
** Copyright 2018 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/


#include <stdint.h>
#include "files.h"
#include "md5.h"
#include "doomtype.h"

struct MD5Check
{
	int length;
	const char *md5;
};

// If these lists grow they should maybe put in gzdoom.pk3.

// The first list is alpha textures in patch format.
// Patches are normally paletted but for these it needs to be treated
// as a grayscale ramp.
static const MD5Check alphapatches[] = 
{
{   18, "fb1472adb34ce1de7a40156d484dd3fc"}, // xhair5.gfx
{   59, "7149115092f5e8f01f84dacc120aae94"}, // xhairs9.gfx
{   62, "7a8d11c24b51e7daff2ab0ffbe3641d9"}, // xhair1.gfx
{   75, "53ca06706de386ec440a501156dd4bc6"}, // xhair2.gfx
{   78, "299bd69e46b4ba3632e8bf5a96f1b0cb"}, // xhair3.gfx
{   86, "643535b5db4361db4177978152294a01"}, // xhairs14.gfx
{   89, "773bb976dd343266bfe2fc11c1d2a2aa"}, // xhairs13.gfx
{  100, "493dd248c2cb0d45635ef9cc995db952"}, // xhair4.gfx
{  105, "257a140b3533713fb59d9be493ba6803"}, // chip4.gfx
{  105, "7e4b578c7217b7f0c7120d62177dd5bc"}, // xhairs8.gfx
{  111, "fd9fec58b188284ece38e43c823abd73"}, // chip2.gfx
{  119, "d6fd267eb249ce01d06fe3e2361eab64"}, // chip3.gfx
{  122, "4daba1da79b0a009b46e2949a2f8c6f8"}, // chip5.gfx
{  128, "ed83d41bcf4a91968ffc3ca5cad6a383"}, // chip1.gfx
{  133, "ca55c89aac30bc8f7e5722520389209e"}, // chip2(2).gfx
{  134, "2b00983c62e69cad182a5204df163701"}, // xhairb9.gfx
{  137, "4fb1b26ac278de9ef382e8268cc7e7bb"}, // xhairb8.gfx
{  138, "b3b69ce27c4b57912232bf718768390a"}, // chip3(2).gfx
{  147, "ae5625169660aaf44ddffe5553be3e28"}, // xhairs11.gfx
{  149, "e594798cfad078a553cd21ea6544abac"}, // chip1(2).gfx
{  152, "bd30eccd4e6afe596af8823d4f8ccf40"}, // xhairb14.gfx
{  153, "5f2111f1b44cc8cd153e6c08eec3f68c"}, // xhairs10.gfx
{  160, "27682ca38c949072848c186f499bf027"}, // chip4(2).gfx
{  168, "7f3d0cf78df24473ffa98ff41d8c93ce"}, // xhairs15.gfx
{  169, "a0df55d2aee6f72c9164127f8fc9f0dd"}, // ahit.gfx
{  171, "08c3d1b4bcb5643ae133938778e74c6f"}, // xhairb11.gfx
{  171, "603355244602e830a2fa9949711a7e83"}, // xhairs12.gfx
{  177, "6dae9b9bd34805cb066309679f9aaf81"}, // bal7scr1.gfx
{  177, "a3312ae7011faf08e7903c753e7108b0"}, // xhair6.gfx
{  183, "142acb37f179aff6e91b430415189c5f"}, // xhair7.gfx
{  185, "ce1428acd24ced861aa628b4fcefc761"}, // xhairb13.gfx
{  194, "ab742500964a2298076a4e3ca5f8d8dc"}, // chip5(2).gfx
{  196, "36e6dd0c675ceb94304954fb1d321f87"}, // xhair8(2).gfx
{  196, "48ef0cdb8c25e6993e38ddedb0b1a69d"}, // xhairb10.gfx
{  239, "72384cc34d4345de824e9b775e185155"}, // xhair8.gfx
{  251, "f3ba8dc69906efee68e27bc37ad8b164"}, // bsplat08_2.gfx
{  251, "f9138bed705dd05b7ac925751f2173c8"}, // bsplat08.gfx
{  261, "c6325f4f79f3e36d00eced5d4a160e50"}, // xhairs17.gfx
{  268, "319f297738ecd5b8e9c32d635c5dc781"}, // bsplat14.gfx
{  268, "b6e71e5e2a5df85f5a1496ff322cda76"}, // bsplat14_2.gfx
{  270, "5fe0a8e06182c155fb0508aae1e2c661"}, // xhairb1.gfx
{  288, "ceb3a910c338fe4f7cdc6c3b49bcb986"}, // xhairb15.gfx
{  290, "5c8810d3be4b27fdbb7660af04ef91ce"}, // xhairb12.gfx
{  307, "53564f95e3bcd9f84f463cdb06d4c959"}, // xhairs18.gfx
{  329, "943cda6f9fbc13bef59c02d38481c1b9"}, // xhairs19.gfx
{  330, "397071348b313cbf4631e16e292a2816"}, // xhair8(3).gfx
{  334, "a7efbab516ae51271d7dece02ee7a3da"}, // xhairs16.gfx
{  341, "ae1f343cbea6ab7892bcc25bd544b149"}, // xhairs20.gfx
{  359, "3828fb4462f3c559d6a1d5659d736d38"}, // Ascrd1.gfx
{  397, "3a3a887506a77d812f07ddb46e13d828"}, // Ascrd2.gfx
{  399, "80a0b9b54618a94fdbd9ff907eb69f2c"}, // Ascrd3.gfx
{  412, "78cef7b3872c4a47bc47155e3fdc2f45"}, // Ascrd4.gfx
{  438, "2c08ca266a7df57d609d85b774fb589f"}, // xhairb2.gfx
{  480, "1f84994c04f4a4941ffc183bda3d9ce3"}, // xhairb17.gfx
{  521, "833d10c6a1b0ed1cf76ce0012628415d"}, // bsplat07_2.gfx
{  521, "8658b919b52228d740c057c50038eaac"}, // bsplat07.gfx
{  521, "c9650cf2f9564b149fbc8c1bbd13c157"}, // Bsplat6.GFX
{  542, "ddc08f004ee63eeec2fbb474393ec5ed"}, // xhairb19.gfx
{  569, "3baa1f2b6c05ceb60e5a7ed5f8990263"}, // bsplat13_2.gfx
{  569, "a1951138cfa47d4ee1033ae088f7ed04"}, // bsplat13.gfx
{  642, "0f4320a0c53f645d2c3551eeea5ba898"}, // Bsplat5.GFX
{  642, "968c486e2f2880a3a8d5c7df87a3d8ad"}, // bsplat06.gfx
{  642, "c41089a518b28be1db182a9d1e7fa7de"}, // bsplat06_2.gfx
{  653, "7bd11e17dd6672680f5325a4f5496c98"}, // bsplat11.gfx
{  653, "b57e64f5f98b1fea132a5d6572f24724"}, // bsplat11_2.gfx
{  653, "e53b9967ef7f93f2ea069b30d6b42444"}, // Bsplat9.GFX
{  780, "0f95bd26c68d0f3180527aa8dc539d28"}, // Ascra2.gfx
{  780, "3e0075c2e707083e83c0b897a20d6089"}, // Fdbal2.gfx
{  801, "3dd3a4bb095deffd70489b9bb6636c96"}, // bsplat12_2.gfx
{  801, "44703476a6702522047fe0ff0831cc31"}, // bsplat12.gfx
{  801, "c4a348a488d948dcc70faa6a396b6625"}, // Fdbal1.gfx
{  801, "f22f176a5e0e2d608c33e2572b28a841"}, // Ascra1.gfx
{  855, "5385652621b2df49fedd6d970b1d582f"}, // bsplat10_2.gfx
{  855, "559817e4658047519abbc58a14b2c97c"}, // Bsplat8.GFX
{  855, "6dee11141dbedff291b9393677276b54"}, // bsplat10.gfx
{  864, "0300a526b884a27d68d22291b926c1d1"}, // xhairb20.gfx
{  900, "0fca1f72e22661c39ba0f4e4e9a39c87"}, // bsplat05.gfx
{  900, "700c0e4cbf0087392e026ac8ab01283d"}, // bsplat05_2.gfx
{  900, "f414ff86af1611bb90043ca859eb2eda"}, // Bsplat4.GFX
{  970, "b9ebf0a5895fd2fcadb3304a8f7351b1"}, // xhairb16.gfx
{ 1020, "08d4691ac72069c5a0897650e91a34b8"}, // bsplat04_2.gfx
{ 1020, "3559fb14d0a30f5fae9705f67c9e2b83"}, // bsplat04.gfx
{ 1020, "afe88bbb055060e39e6e757aebd29a96"}, // Bsplat3.GFX
{ 1034, "8671f2be0d2be65340b7a3b819d57933"}, // xhairb18.gfx
{ 1124, "4ec6d839eb2c10a33dfa0191b2200ede"}, // splat.gfx
{ 1149, "5a0040d28c3f84ef895806a3e80d49a6"}, // bsplat01_2.gfx
{ 1149, "783e85a5195ba91667491002a80e03a3"}, // bsplat01.gfx
{ 1189, "295e144b6318abe47b14d1e6bcc7a7fa"}, // bsplat02_2.gfx
{ 1189, "78606aefd9807b51760b4e6dd9203afa"}, // Bsplat1.GFX
{ 1189, "892d3a9fc35e14fd97f526c8b6ea9a3f"}, // bsplat02.gfx
{ 1218, "3bafcd6672b95d1e4fc92367ed10732d"}, // bsplat03_2.gfx
{ 1218, "916964ed55798ebfd288a65ffb09bb02"}, // bsplat03.gfx
{ 1218, "a3f4fca1e8050d7e5d8e64f48bc16515"}, // Bsplat2.GFX
{ 1235, "7927d6dd72ff5dd3b4b0f538fe78d7b9"}, // bsplat09_2.gfx
{ 1235, "d62f324008e74bfea74efd160b668872"}, // bsplat09.gfx
{ 1235, "f6a0df424b064ed6f6744d5891936ab2"}, // Bsplat7.GFX
{ 1717, "df6d834ad357b48feaa9e9d2b0a1f449"}, // bsplat3(2).gfx
{ 1856, "9a9a2cc871a0dec902c8c91e631bc9c8"}, // Snbal2.gfx
{ 1856, "c1e539d6a93a952a5752018d65d577f4"}, // plasma1.gfx
{ 1856, "e60306eefd66bceef1c802732b9163e3"}, // Snbal4.gfx
{ 1942, "84974f010afdabd9351163175b19bb49"}, // Ascrg2.gfx
{ 1942, "a4caf2b9ade6467ea0018bd78321ff50"}, // uscrch2.gfx
{ 1952, "3ee4ddc539059b533c0e31afbf07bb7f"}, // uscrch1.gfx
{ 1952, "e868f1c2bab9752a6cd819b7a7ce6487"}, // Ascrg1.gfx
{ 2035, "36b4e593a7a60d100e4ec081a7d2f809"}, // Ascrc1.gfx
{ 2080, "ebb5c3017a9beb76258f7d374e99a339"}, // bsplat2(2).gfx
{ 2092, "5461be1e0b02d00ff2204ee457fc5d7c"}, // Ascrc2.gfx
{ 2247, "35f936c271a190e3f67aaea1f02dee57"}, // Snbal3.gfx
{ 2247, "3e237bb101d4e3236a5052a1bc5c80d2"}, // plasma2.gfx
{ 2247, "58b4e240b1c5d41e6150df6e320c1349"}, // Snbal1.gfx
{ 2280, "4e1369d2d683d757445a3d78ace82e98"}, // Ascrh2.gfx
{ 2302, "351c1d1cd32637a1d0b4e0168165b266"}, // Ascrh1.gfx
{ 2821, "8c822e94db02cc9d6c851a189c77123e"}, // xhairs51.gfx
{ 2834, "d0099d0fc5aebcdd3994a45ff2560dbc"}, // xhairs50.gfx
{ 2865, "5c0350860fcb89ed28041d6ab0d023a8"}, // xhairs52.gfx
{ 4370, "bda605422fb9144f9671c03405e7c73b"}, // bsmear2_2.gfx
{ 4370, "f3862fb4f336506542e7e9a91bc88261"}, // bsmear2.gfx
{ 4483, "d05e14fbce5864b2df4b92599c48a4c5"}, // Ascre1.gfx
{ 4506, "230ee2c1a8c5baeafbea7248659da3f4"}, // bsplat1(2).gfx
{ 4855, "d27a510aa0285f1c74cb12c7f8ac6d25"}, // bsplat4(2).gfx
{ 6232, "9e7f0b2c6ca50aa8335929c959ba8c51"}, // Ascrf1.gfx
{ 6858, "876c130f90700587b1a71d1aab095746"}, // bsmear1_2.gfx
{ 6858, "c72d6f4c3a0ff3a2d2db1082aea1c884"}, // bsmear1.gfx
{ 7444, "81e4a147ce264394fad80cfd04965d5d"}, // xhairb50.gfx
{ 7452, "aa3f72a47fe2b7c3e353ea62c75d6b42"}, // xhairb51.gfx
{ 7576, "ac2b9c1b53e91bbb0cb260665189291b"}, // xhairb52.gfx
{10164, "3c8908619cd5b357de701a0c6b04f408"}, // splat1.gfx
{   -1, nullptr}
};

// These are IMGZ images that use the game palette.
// Normally IMGZ is only used for crosshairs, i.e grayscale images.
static const MD5Check palimgzs[] = 
{
{ 1728, "9ffdd749b414a1d34c74650828ad6504"}, // scodef.imgz
{64024, "bee2a3ddec08bf975353b3dc139bb956"}, // interpic.imgz
{   -1, nullptr}
};

//==========================================================================
//
//
//
//==========================================================================

void makeMD5(const void *buffer, unsigned length, char *md5out)
{
	MD5Context md5;
	uint8_t cksum[16];

	md5.Update((uint8_t*)buffer, length);
	md5.Final(cksum);
	for (int i = 0; i < 16; i++)
	{
		mysnprintf(md5out + 2*i, 3, "%02x", cksum[i]);
	}
}

//==========================================================================
//
//
//
//==========================================================================

bool checkPatchForAlpha(const void *buffer, uint32_t length)
{
	if (length > 10164) return false;	// shortcut for anything too large
	
	char md5[33];
	bool done = false;
	
	for(int i=0; alphapatches[i].length > 0; i++)
	{
		if (alphapatches[i].length == (int)length)	// length check
		{
			if (!done) makeMD5(buffer, length, md5);
			done = true;
			if (!memcmp(md5, alphapatches[i].md5, 32))
			{
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

bool checkIMGZPalette(FileReader &file)
{
	uint32_t length = (uint32_t)file.GetLength();
	char md5[33];
	bool done = false;
	for(int i=0; palimgzs[i].length > 0; i++)
	{
		if (palimgzs[i].length ==(int) length)
		{
			if (!done)
			{
				uint8_t *buffer = new uint8_t[length];
				file.Seek(0, FileReader::SeekSet);
				file.Read(buffer, length);
				makeMD5(buffer, length, md5);
				done = true;
				delete[] buffer;
			}
			if (!memcmp(md5, palimgzs[i].md5, 32))
			{
				return true;
			}
		}
	}
	return false;
}
