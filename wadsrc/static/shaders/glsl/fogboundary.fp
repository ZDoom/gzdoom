in vec4 pixelpos;
layout(location=0) out vec4 FragColor;

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
	if (uFogEnabled == -1) 
	{
		fogdist = pixelpos.w;
	}
	else 
	{
		fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
	}
	fogfactor = exp2 (uFogDensity * fogdist);
	FragColor = vec4(uFogColor.rgb, 1.0 - fogfactor);
}

