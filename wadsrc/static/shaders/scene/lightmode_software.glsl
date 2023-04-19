
float SoftwareColormap(float light, float z)
{
	float L = light * 255.0;
	float vis = min(uGlobVis / z, 24.0 / 32.0);
	float shade = 2.0 - (L + 12.0) / 128.0;
	float lightscale = shade - vis;
	return lightscale * 31.0;
}

vec4 Lightmode_Software()
{
	// z is the depth in view space, positive going into the screen
	#if defined(SWLIGHT_RADIAL)
		float z = distance(pixelpos.xyz, uCameraPos.xyz);
	#else
		float z = pixelpos.w;
	#endif

	float colormap = SoftwareColormap(uLightLevel, z);

	#if defined(SWLIGHT_BANDED)
		colormap = floor(colormap) + 0.5;
	#endif

	// Result is the normalized colormap index (0 bright .. 1 dark)
	float newlightlevel = 1.0 - clamp(colormap, 0.0, 31.0) / 32.0;

	vec4 color = vColor;
	color.rgb *= newlightlevel;
	return color;
}
