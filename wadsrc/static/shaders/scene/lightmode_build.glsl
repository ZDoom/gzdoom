
vec4 Lightmode_Build()
{
	// z is the depth in view space, positive going into the screen
	#if defined(SWLIGHT_RADIAL)
		float z = distance(pixelpos.xyz, uCameraPos.xyz);
	#else
		float z = pixelpos.w;
	#endif

	// This is a lot more primitive than Doom's lighting...
	float numShades = float(uPalLightLevels & 255);
	float curshade = (1.0 - uLightLevel) * (numShades - 1.0);
	float visibility = max(uGlobVis * uLightFactor * z, 0.0);
	float shade = clamp((curshade + visibility), 0.0, numShades - 1.0);
	float newlightlevel = 1.0 - clamp(shade * uLightDist, 0.0, 1.0);

	vec4 color = vColor;
	color.rgb *= newlightlevel;
	return color;
}
