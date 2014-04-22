uniform float clipheight;
uniform int fogenabled;
uniform vec4 fogcolor;
uniform vec3 camerapos;
in vec4 pixelpos;
in vec4 fogparm;

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
	if (pixelpos.y > clipheight + 65536.0) discard;
	if (pixelpos.y < clipheight - 65536.0) discard;
#endif

	float fogdist;
	float fogfactor;
	
	//
	// calculate fog factor
	//
	if (fogenabled == -1) 
	{
		fogdist = pixelpos.w;
	}
	else 
	{
		fogdist = max(16.0, distance(pixelpos.xyz, camerapos));
	}
	fogfactor = exp2 (fogparm.z * fogdist);
	gl_FragColor = vec4(fogcolor.rgb, 1.0 - fogfactor);
}

