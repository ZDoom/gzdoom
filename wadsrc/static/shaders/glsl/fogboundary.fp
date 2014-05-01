in vec4 fogcolor;
in vec4 pixelpos;
in float fogdensity;

//===========================================================================
//
// Main shader routine
//
//===========================================================================

void main()
{
#ifndef NO_DISCARD
	// clip plane emulation for plane reflections. These are always perfectly horizontal so a simple check of the pixelpos's y coordinate is sufficient.
	// this setup is designed to perform this check with as few operations and values as possible.
	if (pixelpos.y > uClipHeight + 65536.0) discard;
	if (pixelpos.y < uClipHeight - 65536.0) discard;
#endif

	float fogdist;
	float fogfactor;
	
	//
	// calculate fog factor
	//
	if (uFogMode == 1) 
	{
		fogdist = pixelpos.w;
	}
	else 
	{
		fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
	}
	fogfactor = exp2 (fogdensity * fogdist);
	gl_FragColor = vec4(fogcolor.rgb, 1.0 - fogfactor);
}

