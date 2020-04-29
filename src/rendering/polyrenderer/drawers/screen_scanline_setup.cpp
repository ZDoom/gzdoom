/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stddef.h>
#include "templates.h"
#include "poly_thread.h"
#include "screen_scanline_setup.h"
#include "x86.h"
#include <cmath>

#ifndef NO_SSE
#include <immintrin.h>
#endif

#ifdef NO_SSE
void WriteW(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	float startX = x0 + (0.5f - args->v1->x);
	float startY = y + (0.5f - args->v1->y);

	float posW = args->v1->w + args->gradientX.W * startX + args->gradientY.W * startY;
	float stepW = args->gradientX.W;
	float* w = thread->scanline.W;
	for (int x = x0; x < x1; x++)
	{
		w[x] = 1.0f / posW;
		posW += stepW;
	}
}
#else
void WriteW(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	float startX = x0 + (0.5f - args->v1->x);
	float startY = y + (0.5f - args->v1->y);

	float posW = args->v1->w + args->gradientX.W * startX + args->gradientY.W * startY;
	float stepW = args->gradientX.W;
	float* w = thread->scanline.W;

	int ssecount = ((x1 - x0) & ~3);
	int sseend = x0 + ssecount;

	__m128 mstepW = _mm_set1_ps(stepW * 4.0f);
	__m128 mposW = _mm_setr_ps(posW, posW + stepW, posW + stepW + stepW, posW + stepW + stepW + stepW);

	for (int x = x0; x < sseend; x += 4)
	{
		// One Newton-Raphson iteration for 1/posW
		__m128 res = _mm_rcp_ps(mposW);
		__m128 muls = _mm_mul_ps(mposW, _mm_mul_ps(res, res));
		_mm_storeu_ps(w + x, _mm_sub_ps(_mm_add_ps(res, res), muls));
		mposW = _mm_add_ps(mposW, mstepW);
	}

	posW += ssecount * stepW;
	for (int x = sseend; x < x1; x++)
	{
		w[x] = 1.0f / posW;
		posW += stepW;
	}
}
#endif

static void WriteDynLightArray(int x0, int x1, PolyTriangleThreadData* thread)
{
	int num_lights = thread->numPolyLights;
	PolyLight* lights = thread->polyLights;

	float worldnormalX = thread->mainVertexShader.vWorldNormal.X;
	float worldnormalY = thread->mainVertexShader.vWorldNormal.Y;
	float worldnormalZ = thread->mainVertexShader.vWorldNormal.Z;

	uint32_t* lightarray = thread->scanline.lightarray;
	float* worldposX = thread->scanline.WorldX;
	float* worldposY = thread->scanline.WorldY;
	float* worldposZ = thread->scanline.WorldZ;

	int sseend = x0;

#ifndef NO_SSE
	int ssecount = ((x1 - x0) & ~3);
	sseend = x0 + ssecount;

	__m128 mworldnormalX = _mm_set1_ps(worldnormalX);
	__m128 mworldnormalY = _mm_set1_ps(worldnormalY);
	__m128 mworldnormalZ = _mm_set1_ps(worldnormalZ);

	for (int x = x0; x < sseend; x += 4)
	{
		__m128i lit = _mm_loadu_si128((__m128i*)&lightarray[x]);
		__m128i litlo = _mm_unpacklo_epi8(lit, _mm_setzero_si128());
		__m128i lithi = _mm_unpackhi_epi8(lit, _mm_setzero_si128());

		for (int i = 0; i < num_lights; i++)
		{
			__m128 lightposX = _mm_set1_ps(lights[i].x);
			__m128 lightposY = _mm_set1_ps(lights[i].y);
			__m128 lightposZ = _mm_set1_ps(lights[i].z);
			__m128 light_radius = _mm_set1_ps(lights[i].radius);
			__m128i light_color = _mm_shuffle_epi32(_mm_unpacklo_epi8(_mm_cvtsi32_si128(lights[i].color), _mm_setzero_si128()), _MM_SHUFFLE(1, 0, 1, 0));

			__m128 is_attenuated = _mm_cmplt_ps(light_radius, _mm_setzero_ps());
			light_radius = _mm_andnot_ps(_mm_set1_ps(-0.0f), light_radius); // clear sign bit

			// L = light-pos
			// dist = sqrt(dot(L, L))
			// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
			__m128 Lx = _mm_sub_ps(lightposX, _mm_loadu_ps(&worldposX[x]));
			__m128 Ly = _mm_sub_ps(lightposY, _mm_loadu_ps(&worldposY[x]));
			__m128 Lz = _mm_sub_ps(lightposZ, _mm_loadu_ps(&worldposZ[x]));
			__m128 dist2 = _mm_add_ps(_mm_mul_ps(Lx, Lx), _mm_add_ps(_mm_mul_ps(Ly, Ly), _mm_mul_ps(Lz, Lz)));
			__m128 rcp_dist = _mm_rsqrt_ps(dist2);
			__m128 dist = _mm_mul_ps(dist2, rcp_dist);
			__m128 distance_attenuation = _mm_sub_ps(_mm_set1_ps(256.0f), _mm_min_ps(_mm_mul_ps(dist, light_radius), _mm_set1_ps(256.0f)));

			// The simple light type
			__m128 simple_attenuation = distance_attenuation;

			// The point light type
			// diffuse = max(dot(N,normalize(L)),0) * attenuation
			Lx = _mm_mul_ps(Lx, rcp_dist);
			Ly = _mm_mul_ps(Ly, rcp_dist);
			Lz = _mm_mul_ps(Lz, rcp_dist);
			__m128 dotNL = _mm_add_ps(_mm_add_ps(_mm_mul_ps(mworldnormalX, Lx), _mm_mul_ps(mworldnormalY, Ly)), _mm_mul_ps(mworldnormalZ, Lz));
			__m128 point_attenuation = _mm_mul_ps(_mm_max_ps(dotNL, _mm_setzero_ps()), distance_attenuation);

			__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, point_attenuation), _mm_andnot_ps(is_attenuated, simple_attenuation)));

			attenuation = _mm_shufflehi_epi16(_mm_shufflelo_epi16(attenuation, _MM_SHUFFLE(2, 2, 0, 0)), _MM_SHUFFLE(2, 2, 0, 0));
			__m128i attenlo = _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1, 1, 0, 0));
			__m128i attenhi = _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(3, 3, 2, 2));

			litlo = _mm_add_epi16(litlo, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenlo), 8));
			lithi = _mm_add_epi16(lithi, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenhi), 8));
		}

		_mm_storeu_si128((__m128i*)&lightarray[x], _mm_packus_epi16(litlo, lithi));
	}
#endif

	for (int x = sseend; x < x1; x++)
	{
		uint32_t lit_a = APART(lightarray[x]);
		uint32_t lit_r = RPART(lightarray[x]);
		uint32_t lit_g = GPART(lightarray[x]);
		uint32_t lit_b = BPART(lightarray[x]);

		for (int i = 0; i < num_lights; i++)
		{
			float lightposX = lights[i].x;
			float lightposY = lights[i].y;
			float lightposZ = lights[i].z;
			float light_radius = lights[i].radius;
			uint32_t light_color = lights[i].color;

			bool is_attenuated = light_radius < 0.0f;
			if (is_attenuated)
				light_radius = -light_radius;

			// L = light-pos
			// dist = sqrt(dot(L, L))
			// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
			float Lx = lightposX - worldposX[x];
			float Ly = lightposY - worldposY[x];
			float Lz = lightposZ - worldposZ[x];
			float dist2 = Lx * Lx + Ly * Ly + Lz * Lz;
#ifdef NO_SSE
			//float rcp_dist = 1.0f / sqrt(dist2);
			float rcp_dist = 1.0f / (dist2 * 0.01f);
#else
			float rcp_dist = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(dist2)));
#endif
			float dist = dist2 * rcp_dist;
			float distance_attenuation = 256.0f - MIN(dist * light_radius, 256.0f);

			// The simple light type
			float simple_attenuation = distance_attenuation;

			// The point light type
			// diffuse = max(dot(N,normalize(L)),0) * attenuation
			Lx *= rcp_dist;
			Ly *= rcp_dist;
			Lz *= rcp_dist;
			float dotNL = worldnormalX * Lx + worldnormalY * Ly + worldnormalZ * Lz;
			float point_attenuation = MAX(dotNL, 0.0f) * distance_attenuation;

			uint32_t attenuation = (uint32_t)(is_attenuated ? (int32_t)point_attenuation : (int32_t)simple_attenuation);

			lit_r += (RPART(light_color) * attenuation) >> 8;
			lit_g += (GPART(light_color) * attenuation) >> 8;
			lit_b += (BPART(light_color) * attenuation) >> 8;
		}

		lit_r = MIN<uint32_t>(lit_r, 255);
		lit_g = MIN<uint32_t>(lit_g, 255);
		lit_b = MIN<uint32_t>(lit_b, 255);
		lightarray[x] = MAKEARGB(lit_a, lit_r, lit_g, lit_b);

		// Palette version:
		// dynlights[x] = RGB256k.All[((lit_r >> 2) << 12) | ((lit_g >> 2) << 6) | (lit_b >> 2)];
	}
}

static void WriteLightArray(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	auto constants = thread->PushConstants;

	auto vColorR = thread->scanline.vColorR;
	auto vColorG = thread->scanline.vColorG;
	auto vColorB = thread->scanline.vColorB;
	auto vColorA = thread->scanline.vColorA;

	if (thread->PushConstants->uLightLevel >= 0.0f)
	{
		float startX = x0 + (0.5f - args->v1->x);
		float startY = y + (0.5f - args->v1->y);
		float posW = args->v1->w + args->gradientX.W * startX + args->gradientY.W * startY;
		float stepW = args->gradientX.W;

		float globVis = thread->mainVertexShader.Viewpoint->mGlobVis;

		uint32_t light = (int)(constants->uLightLevel * 255.0f);
		fixed_t shade = (fixed_t)((2.0f - (light + 12.0f) / 128.0f) * (float)FRACUNIT);
		fixed_t lightpos = (fixed_t)(globVis * posW * (float)FRACUNIT);
		fixed_t lightstep = (fixed_t)(globVis * stepW * (float)FRACUNIT);

		fixed_t maxvis = 24 * FRACUNIT / 32;
		fixed_t maxlight = 31 * FRACUNIT / 32;

		fixed_t lightend = lightpos + lightstep * (x1 - x0);
		if (lightpos < maxvis && shade >= lightpos && shade - lightpos <= maxlight &&
			lightend < maxvis && shade >= lightend && shade - lightend <= maxlight)
		{
			lightpos += FRACUNIT - shade;
			uint32_t* lightarray = thread->scanline.lightarray;
			for (int x = x0; x < x1; x++)
			{
				uint32_t l = MIN(lightpos >> 8, 256);

				uint32_t r = vColorR[x];
				uint32_t g = vColorG[x];
				uint32_t b = vColorB[x];
				uint32_t a = vColorA[x];

				lightarray[x] = MAKEARGB(a, (r * l) >> 8, (g * l) >> 8, (b * l) >> 8);
				lightpos += lightstep;
			}
		}
		else
		{
			uint32_t* lightarray = thread->scanline.lightarray;
			for (int x = x0; x < x1; x++)
			{
				uint32_t l = MIN((FRACUNIT - clamp<fixed_t>(shade - MIN(maxvis, lightpos), 0, maxlight)) >> 8, 256);
				uint32_t r = vColorR[x];
				uint32_t g = vColorG[x];
				uint32_t b = vColorB[x];
				uint32_t a = vColorA[x];

				lightarray[x] = MAKEARGB(a, (r * l) >> 8, (g * l) >> 8, (b * l) >> 8);
				lightpos += lightstep;
			}
		}
	}
	else if (constants->uFogEnabled > 0)
	{
		float uLightDist = constants->uLightDist;
		float uLightFactor = constants->uLightFactor;
		float* w = thread->scanline.W;
		uint32_t* lightarray = thread->scanline.lightarray;
		for (int x = x0; x < x1; x++)
		{
			uint32_t a = thread->scanline.vColorA[x];
			uint32_t r = thread->scanline.vColorR[x];
			uint32_t g = thread->scanline.vColorG[x];
			uint32_t b = thread->scanline.vColorB[x];

			float fogdist = MAX(16.0f, w[x]);
			float fogfactor = std::exp2(constants->uFogDensity * fogdist);

			// brightening around the player for light mode 2:
			if (fogdist < uLightDist)
			{
				uint32_t l = (int)((uLightFactor - (fogdist / uLightDist) * (uLightFactor - 1.0)) * 256.0f);
				r = (r * l) >> 8;
				g = (g * l) >> 8;
				b = (b * l) >> 8;
			}

			// apply light diminishing through fog equation: mix(vec3(0.0, 0.0, 0.0), lightshade.rgb, fogfactor)
			uint32_t t = (int)(fogfactor * 256.0f);
			r = (r * t) >> 8;
			g = (g * t) >> 8;
			b = (b * t) >> 8;

			lightarray[x] = MAKEARGB(a, r, g, b);
		}
	}
	else
	{
		uint32_t* lightarray = thread->scanline.lightarray;
		for (int x = x0; x < x1; x++)
		{
			uint32_t a = thread->scanline.vColorA[x];
			uint32_t r = thread->scanline.vColorR[x];
			uint32_t g = thread->scanline.vColorG[x];
			uint32_t b = thread->scanline.vColorB[x];
			lightarray[x] = MAKEARGB(a, r, g, b);
		}
	}
}

#ifdef NO_SSE
static void WriteVarying(float pos, float step, int x0, int x1, const float* w, float* varying)
{
	for (int x = x0; x < x1; x++)
	{
		varying[x] = pos * w[x];
		pos += step;
	}
}
#else
static void WriteVarying(float pos, float step, int x0, int x1, const float* w, float* varying)
{
	int ssecount = ((x1 - x0) & ~3);
	int sseend = x0 + ssecount;

	__m128 mstep = _mm_set1_ps(step * 4.0f);
	__m128 mpos = _mm_setr_ps(pos, pos + step, pos + step + step, pos + step + step + step);

	for (int x = x0; x < sseend; x += 4)
	{
		_mm_storeu_ps(varying + x, _mm_mul_ps(mpos, _mm_loadu_ps(w + x)));
		mpos = _mm_add_ps(mpos, mstep);
	}

	pos += ssecount * step;
	for (int x = sseend; x < x1; x++)
	{
		varying[x] = pos * w[x];
		pos += step;
	}
}
#endif

#ifdef NO_SSE
static void WriteVaryingWrap(float pos, float step, int x0, int x1, const float* w, uint16_t* varying)
{
	for (int x = x0; x < x1; x++)
	{
		float value = pos * w[x];
		value = value - std::floor(value);
		varying[x] = static_cast<uint32_t>(static_cast<int32_t>(value * static_cast<float>(0x1000'0000)) << 4) >> 16;
		pos += step;
	}
}
#else
static void WriteVaryingWrap(float pos, float step, int x0, int x1, const float* w, uint16_t* varying)
{
	int ssecount = ((x1 - x0) & ~3);
	int sseend = x0 + ssecount;

	__m128 mstep = _mm_set1_ps(step * 4.0f);
	__m128 mpos = _mm_setr_ps(pos, pos + step, pos + step + step, pos + step + step + step);

	for (int x = x0; x < sseend; x += 4)
	{
		__m128 value = _mm_mul_ps(mpos, _mm_loadu_ps(w + x));
		__m128 f = value;
		__m128 t = _mm_cvtepi32_ps(_mm_cvttps_epi32(f));
		__m128 r = _mm_sub_ps(t, _mm_and_ps(_mm_cmplt_ps(f, t), _mm_set1_ps(1.0f)));
		value = _mm_sub_ps(f, r);

		__m128i ivalue = _mm_srli_epi32(_mm_slli_epi32(_mm_cvttps_epi32(_mm_mul_ps(value, _mm_set1_ps(static_cast<float>(0x1000'0000)))), 4), 17);
		_mm_storel_epi64((__m128i*)(varying + x), _mm_slli_epi16(_mm_packs_epi32(ivalue, ivalue), 1));
		mpos = _mm_add_ps(mpos, mstep);
	}

	pos += ssecount * step;
	for (int x = sseend; x < x1; x++)
	{
		float value = pos * w[x];
		__m128 f = _mm_set_ss(value);
		__m128 t = _mm_cvtepi32_ps(_mm_cvttps_epi32(f));
		__m128 r = _mm_sub_ss(t, _mm_and_ps(_mm_cmplt_ps(f, t), _mm_set_ss(1.0f)));
		value = _mm_cvtss_f32(_mm_sub_ss(f, r));

		varying[x] = static_cast<uint32_t>(static_cast<int32_t>(value * static_cast<float>(0x1000'0000)) << 4) >> 16;
		pos += step;
	}
}
#endif

static void WriteVaryingWarp1(float posU, float posV, float stepU, float stepV, int x0, int x1, PolyTriangleThreadData* thread)
{
	float pi2 = 3.14159265358979323846f * 2.0f;
	float timer = thread->mainVertexShader.Data.timer * 0.125f;

	const float* w = thread->scanline.W;
	uint16_t* scanlineU = thread->scanline.U;
	uint16_t* scanlineV = thread->scanline.V;

	for (int x = x0; x < x1; x++)
	{
		float u = posU * w[x];
		float v = posV * w[x];

		v += (float)g_sin(pi2 * (u + timer)) * 0.1f;
		u += (float)g_sin(pi2 * (v + timer)) * 0.1f;

		u = u - std::floor(u);
		v = v - std::floor(v);
		scanlineU[x] = static_cast<uint32_t>(static_cast<int32_t>(u * static_cast<float>(0x1000'0000)) << 4) >> 16;
		scanlineV[x] = static_cast<uint32_t>(static_cast<int32_t>(v * static_cast<float>(0x1000'0000)) << 4) >> 16;

		posU += stepU;
		posV += stepV;
	}
}

static void WriteVaryingWarp2(float posU, float posV, float stepU, float stepV, int x0, int x1, PolyTriangleThreadData* thread)
{
	float pi2 = 3.14159265358979323846f * 2.0f;
	float timer = thread->mainVertexShader.Data.timer;

	const float* w = thread->scanline.W;
	uint16_t* scanlineU = thread->scanline.U;
	uint16_t* scanlineV = thread->scanline.V;

	for (int x = x0; x < x1; x++)
	{
		float u = posU * w[x];
		float v = posV * w[x];

		v += (0.5f + (float)g_sin(pi2 * (v + timer * 0.61f + 900.f/8192.f)) + (float)g_sin(pi2 * (u * 2.0f + timer * 0.36f + 300.0f/8192.0f))) * 0.025f;
		u += (0.5f + (float)g_sin(pi2 * (v + timer * 0.49f + 700.f/8192.f)) + (float)g_sin(pi2 * (u * 2.0f + timer * 0.49f + 1200.0f/8192.0f))) * 0.025f;

		u = u - std::floor(u);
		v = v - std::floor(v);
		scanlineU[x] = static_cast<uint32_t>(static_cast<int32_t>(u * static_cast<float>(0x1000'0000)) << 4) >> 16;
		scanlineV[x] = static_cast<uint32_t>(static_cast<int32_t>(v * static_cast<float>(0x1000'0000)) << 4) >> 16;

		posU += stepU;
		posV += stepV;
	}
}

#ifdef NO_SSE
static void WriteVaryingColor(float pos, float step, int x0, int x1, const float* w, uint8_t* varying)
{
	for (int x = x0; x < x1; x++)
	{
		varying[x] = clamp(static_cast<int>(pos * w[x] * 255.0f), 0, 255);
		pos += step;
	}
}
#else
static void WriteVaryingColor(float pos, float step, int x0, int x1, const float* w, uint8_t* varying)
{
	int ssecount = ((x1 - x0) & ~3);
	int sseend = x0 + ssecount;

	__m128 mstep = _mm_set1_ps(step * 4.0f);
	__m128 mpos = _mm_setr_ps(pos, pos + step, pos + step + step, pos + step + step + step);

	for (int x = x0; x < sseend; x += 4)
	{
		__m128i value = _mm_cvttps_epi32(_mm_mul_ps(_mm_mul_ps(mpos, _mm_loadu_ps(w + x)), _mm_set1_ps(255.0f)));
		value = _mm_packs_epi32(value, value);
		value = _mm_packus_epi16(value, value);
		*(uint32_t*)(varying + x) = _mm_cvtsi128_si32(value);
		mpos = _mm_add_ps(mpos, mstep);
	}

	pos += ssecount * step;
	for (int x = sseend; x < x1; x++)
	{
		varying[x] = clamp(static_cast<int>(pos * w[x] * 255.0f), 0, 255);
		pos += step;
	}
}
#endif

void WriteVaryings(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	float startX = x0 + (0.5f - args->v1->x);
	float startY = y + (0.5f - args->v1->y);

	void (*useShader)(float posU, float posV, float stepU, float stepV, int x0, int x1, PolyTriangleThreadData* thread) = nullptr;

	if (thread->EffectState == SHADER_Warp1)
		useShader = &WriteVaryingWarp1;
	else if (thread->EffectState == SHADER_Warp2)
		useShader = &WriteVaryingWarp2;

	if (useShader)
	{
		useShader(
			args->v1->u * args->v1->w + args->gradientX.U * startX + args->gradientY.U * startY,
			args->v1->v * args->v1->w + args->gradientX.V * startX + args->gradientY.V * startY,
			args->gradientX.U,
			args->gradientX.V,
			x0, x1,
			thread);
	}
	else
	{
		WriteVaryingWrap(args->v1->u * args->v1->w + args->gradientX.U * startX + args->gradientY.U * startY, args->gradientX.U, x0, x1, thread->scanline.W, thread->scanline.U);
		WriteVaryingWrap(args->v1->v * args->v1->w + args->gradientX.V * startX + args->gradientY.V * startY, args->gradientX.V, x0, x1, thread->scanline.W, thread->scanline.V);
	}
	WriteVarying(args->v1->worldX * args->v1->w + args->gradientX.WorldX * startX + args->gradientY.WorldX * startY, args->gradientX.WorldX, x0, x1, thread->scanline.W, thread->scanline.WorldX);
	WriteVarying(args->v1->worldY * args->v1->w + args->gradientX.WorldY * startX + args->gradientY.WorldY * startY, args->gradientX.WorldY, x0, x1, thread->scanline.W, thread->scanline.WorldY);
	WriteVarying(args->v1->worldZ * args->v1->w + args->gradientX.WorldZ * startX + args->gradientY.WorldZ * startY, args->gradientX.WorldZ, x0, x1, thread->scanline.W, thread->scanline.WorldZ);
	WriteVarying(args->v1->gradientdistZ * args->v1->w + args->gradientX.GradientdistZ * startX + args->gradientY.GradientdistZ * startY, args->gradientX.GradientdistZ, x0, x1, thread->scanline.W, thread->scanline.GradientdistZ);
	WriteVaryingColor(args->v1->a * args->v1->w + args->gradientX.A * startX + args->gradientY.A * startY, args->gradientX.A, x0, x1, thread->scanline.W, thread->scanline.vColorA);
	WriteVaryingColor(args->v1->r * args->v1->w + args->gradientX.R * startX + args->gradientY.R * startY, args->gradientX.R, x0, x1, thread->scanline.W, thread->scanline.vColorR);
	WriteVaryingColor(args->v1->g * args->v1->w + args->gradientX.G * startX + args->gradientY.G * startY, args->gradientX.G, x0, x1, thread->scanline.W, thread->scanline.vColorG);
	WriteVaryingColor(args->v1->b * args->v1->w + args->gradientX.B * startX + args->gradientY.B * startY, args->gradientX.B, x0, x1, thread->scanline.W, thread->scanline.vColorB);

	if (thread->PushConstants->uFogEnabled != -3 && thread->PushConstants->uTextureMode != TM_FOGLAYER)
		WriteLightArray(y, x0, x1, args, thread);

	if (thread->numPolyLights > 0)
		WriteDynLightArray(x0, x1, thread);
}
