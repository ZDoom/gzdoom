#pragma once

#include <cstdint>
#include <cmath>

class Colorf
{
public:
	Colorf() = default;
	Colorf(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) { }

	static Colorf transparent() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }

	static Colorf fromRgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
	{
		float s = 1.0f / 255.0f;
		return { r * s, g * s, b * s, a * s };
	}

	uint32_t toBgra8() const
	{
		uint32_t cr = (int)(std::max(std::min(r * 255.0f, 255.0f), 0.0f));
		uint32_t cg = (int)(std::max(std::min(g * 255.0f, 255.0f), 0.0f));
		uint32_t cb = (int)(std::max(std::min(b * 255.0f, 255.0f), 0.0f));
		uint32_t ca = (int)(std::max(std::min(a * 255.0f, 255.0f), 0.0f));
		return (ca << 24) | (cr << 16) | (cg << 8) | cb;
	}

	bool operator==(const Colorf& v) const { return r == v.r && g == v.g && b == v.b && a == v.a; }
	bool operator!=(const Colorf& v) const { return r != v.r || g != v.g || b != v.b || a != v.a; }

	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 1.0f;
};
