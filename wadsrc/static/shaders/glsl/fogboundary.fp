uniform int fogenabled;
uniform vec4 fogcolor;
uniform vec3 camerapos;
varying vec4 pixelpos;
varying vec4 fogparm;

//===========================================================================
//
// Main shader routine
//
//===========================================================================

void main()
{
	float fogdist;
	float fogfactor;
	
	//
	// calculate fog factor
	//
	#ifndef NO_SM4
		if (fogenabled == -1) 
		{
			fogdist = pixelpos.w;
		}
		else 
		{
			fogdist = max(16.0, distance(pixelpos.xyz, camerapos));
		}
	#else
		fogdist = pixelpos.w;
	#endif
	fogfactor = exp2 (fogparm.z * fogdist);
	gl_FragColor = vec4(fogcolor.rgb, 1.0 - fogfactor);
}

