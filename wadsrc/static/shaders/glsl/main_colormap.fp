
uniform float alphathreshold;
uniform float clipheight;

uniform int texturemode;
uniform sampler2D tex;

uniform vec3 colormapstart;
uniform vec3 colormaprange;
varying vec4 pixelpos;

vec4 Process(vec4 color);


vec4 desaturate(vec4 texel)
{
	return texel;
}

vec4 getTexel(vec2 st)
{
	vec4 texel = texture2D(tex, st);
	
	#ifndef NO_TEXTUREMODE
		//
		// Apply texture modes
		//
		if (texturemode == 2) 
		{
			texel.a = 1.0;
		}
		else if (texturemode == 1) 
		{
			texel.rgb = vec3(1.0,1.0,1.0);
		}
	#endif

	return texel;
}



void main()
{
#ifndef NO_DISCARD
	// clip plane emulation for plane reflections. These are always perfectly horizontal so a simple check of the pixelpos's y coordinate is sufficient.
	// this setup is designed to perform this check with as few operations and values as possible.
	if (pixelpos.y > clipheight + 65536.0) discard;
	if (pixelpos.y < clipheight - 65536.0) discard;
#endif

	vec4 frag = Process(vec4(1.0,1.0,1.0,1.0));
#ifndef NO_DISCARD
	if (frag.a < alphathreshold)
	{
		discard;
	}
#endif
	
	float gray = (frag.r * 0.3 + frag.g * 0.56 + frag.b * 0.14);	
	vec3 cm = colormapstart + gray * colormaprange;
	gl_FragColor = vec4(clamp(cm, 0.0, 1.0), frag.a) * gl_Color;
}

