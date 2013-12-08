uniform int fogenabled;
uniform vec4 fogcolor;
uniform vec3 camerapos;
varying vec4 pixelpos;
varying vec4 fogparm;

uniform sampler2D tex;

vec4 Process(vec4 color);

//===========================================================================
//
// Gets a texel and performs common manipulations
//
//===========================================================================

vec4 desaturate(vec4 texel)
{
	return texel;
}

vec4 getTexel(vec2 st)
{
	vec4 texel = texture2D(tex, st);
	return texel;
}

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
	
	vec4 frag = Process(vec4(1.0,1.0,1.0,1.0));
	gl_FragColor = vec4(fogcolor.rgb, (1.0 - fogfactor) * frag.a * 0.75 * gl_Color.a);
}

