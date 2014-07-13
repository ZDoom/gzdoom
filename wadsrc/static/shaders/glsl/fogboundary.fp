in vec4 pixelpos;
in vec2 glowdist;

//===========================================================================
//
// Main shader routine
//
//===========================================================================

void main()
{
#ifndef NO_DISCARD
	// clip plane emulation for plane reflections. These are always perfectly horizontal so a simple check of the pixelpos's y coordinate is sufficient.
	if (pixelpos.y > uClipHeightTop) discard;
	if (pixelpos.y < uClipHeightBottom) discard;
#endif

	float fogdist;
	float fogfactor;
	
	//
	// calculate fog factor
	//
	if (uFogEnabled == -1) 
	{
		fogdist = pixelpos.w;
	}
	else 
	{
		fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
	}
	fogfactor = exp2 (uFogDensity * fogdist);
	gl_FragColor = vec4(uFogColor.rgb, 1.0 - fogfactor);
}

