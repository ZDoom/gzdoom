uniform int fogenabled;
uniform vec4 fogcolor;
uniform int texturemode;
in vec4 pixelpos;
in vec4 fogparm;

uniform sampler2D tex;
uniform vec4 objectcolor;

vec4 Process(vec4 color);
vec4 ProcessTexel();

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
	
	//
	// Apply texture modes
	//
	switch (texturemode)
	{
		case 1:
			texel.rgb = vec3(1.0,1.0,1.0);
			break;
			
		case 2:
			texel.a = 1.0;
			break;
			
		case 3:
			texel = vec4(1.0, 1.0, 1.0, texel.r);
			break;
	}
	texel *= objectcolor;

	return desaturate(texel);
}

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

	vec4 frag = ProcessTexel();
#ifndef NO_DISCARD
	if (frag.a < uAlphaThreshold)
	{
		discard;
	}
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
	fogfactor = exp2 (fogparm.z * fogdist);
	
	gl_FragColor = vec4(fogcolor.rgb, (1.0 - fogfactor) * frag.a * 0.75 * gl_Color.a);
}

