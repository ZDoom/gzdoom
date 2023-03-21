
#include "shaders/scene/grayscale.glsl"

// Desaturate a color

vec4 dodesaturate(vec4 texel, float factor)
{
	if (factor != 0.0)
	{
		float gray = grayscale(texel);
		return mix (texel, vec4(gray,gray,gray,texel.a), factor);
	}
	else
	{
		return texel;
	}
}

vec4 desaturate(vec4 texel)
{
	return dodesaturate(texel, uDesaturationFactor);
}
