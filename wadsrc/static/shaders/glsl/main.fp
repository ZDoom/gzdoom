// Changing this constant gives results very similar to changing r_visibility.
// Default is 232, it seems to give exactly the same light bands as software renderer.
#define DOOMLIGHTFACTOR 232.0

uniform int fogenabled;
uniform vec4 fogcolor;
uniform vec4 dlightcolor;		// one global light value that will get added just like a dynamic light.
uniform vec4 objectcolor;		// object's own color - not part of the lighting.
uniform vec3 camerapos;
in vec4 pixelpos;
in vec4 fogparm;
//uniform vec2 lightparms;
in float desaturation_factor;
flat in ivec4 lightrange;

in vec4 topglowcolor;
in vec4 bottomglowcolor;
in vec2 glowdist;

uniform int texturemode;
uniform sampler2D tex;

vec4 Process(vec4 color);
vec4 ProcessTexel();
vec4 ProcessLight(vec4 color);


in float lightlevel;

//===========================================================================
//
// Desaturate a color
//
//===========================================================================

vec4 desaturate(vec4 texel)
{
	if (desaturation_factor > 0.0)
	{
		float gray = (texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14);	
		return mix (vec4(gray,gray,gray,texel.a), texel, desaturation_factor);
	}
	else
	{
		return texel;
	}
}

//===========================================================================
//
// This function is common for all (non-special-effect) fragment shaders
//
//===========================================================================

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
// Doom lighting equation ripped from EDGE.
// Big thanks to EDGE developers for making the only port
// that actually replicates software renderer's lighting in OpenGL.
// Float version.
// Basically replace int with float and divide all constants by 31.
//
//===========================================================================

float R_DoomLightingEquation(float light, float dist)
{
	/* L in the range 0 to 63 */
	float L = light * 63.0/31.0;

	float min_L = clamp(36.0/31.0 - L, 0.0, 1.0);

	// Fix objects getting totally black when close.
	if (dist < 0.0001)
		dist = 0.0001;

	float scale = 1.0 / dist;
	float index = (59.0/31.0 - L) - (scale * DOOMLIGHTFACTOR/31.0 - DOOMLIGHTFACTOR/31.0);

	/* result is colormap index (0 bright .. 31 dark) */
	return clamp(index, min_L, 1.0);
}

//===========================================================================
//
// Calculate light
//
// It is important to note that the light color is not desaturated
// due to ZDoom's implementation weirdness. Everything that's added
// on top of it, e.g. dynamic lights and glows are, though, because
// the objects emitting these lights are also.
//
//===========================================================================

vec4 getLightColor(float fogdist, float fogfactor)
{
	vec4 color = gl_Color;
	if (uLightMode == 8)
	{
		float newlightlevel = 1.0 - R_DoomLightingEquation(lightlevel, gl_FragCoord.z);
		color.rgb *= newlightlevel;
	}
	else if (fogenabled > 0)
	{
		// brightening around the player for light mode 2
		if (fogdist < fogparm.y)
		{
			color.rgb *= fogparm.x - (fogdist / fogparm.y) * (fogparm.x - 1.0);
		}
		
		//
		// apply light diminishing through fog equation
		//
		color.rgb = mix(vec3(0.0, 0.0, 0.0), color.rgb, fogfactor);
	}
	
	//
	// handle glowing walls
	//
	if (topglowcolor.a > 0.0 && glowdist.x < topglowcolor.a)
	{
		color.rgb += desaturate(topglowcolor * (1.0 - glowdist.x / topglowcolor.a)).rgb;
	}
	if (bottomglowcolor.a > 0.0 && glowdist.y < bottomglowcolor.a)
	{
		color.rgb += desaturate(bottomglowcolor * (1.0 - glowdist.y / bottomglowcolor.a)).rgb;
	}
	color = min(color, 1.0);

	//
	// apply brightmaps (or other light manipulation by custom shaders.
	//
	color = ProcessLight(color);

	//
	// apply dynamic lights (except additive)
	//
	
	vec4 dynlight = dlightcolor;

	if (lightrange.z > lightrange.x)
	{
		//
		// modulated lights
		//
		for(int i=lightrange.x; i<lightrange.y; i+=2)
		{
			vec4 lightpos = Parameters[i];
			vec4 lightcolor = Parameters[i+1];
			
			lightcolor.rgb *= max(lightpos.w - distance(pixelpos.xyz, lightpos.xyz),0.0) / lightpos.w;
			dynlight.rgb += lightcolor.rgb;
		}
		//
		// subtractive lights
		//
		for(int i=lightrange.y; i<lightrange.z; i+=2)
		{
			vec4 lightpos = Parameters[i];
			vec4 lightcolor = Parameters[i+1];
			
			lightcolor.rgb *= max(lightpos.w - distance(pixelpos.xyz, lightpos.xyz),0.0) / lightpos.w;
			dynlight.rgb -= lightcolor.rgb;
		}
	}
	color.rgb = clamp(color.rgb + desaturate(dynlight).rgb, 0.0, 1.4);
	
	// prevent any unintentional messing around with the alpha.
	return vec4(color.rgb, gl_Color.a);
}

//===========================================================================
//
// Applies colored fog
//
//===========================================================================

vec4 applyFog(vec4 frag, float fogfactor)
{
	return vec4(mix(fogcolor.rgb, frag.rgb, fogfactor), frag.a);
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

	switch (uSpecialMode)
	{
		default:
			switch (uFixedColormap)
			{
				case 0:
				{
					float fogdist = 0.0;
					float fogfactor = 0.0;
					

					//
					// calculate fog factor
					//
					if (fogenabled != 0)
					{
						if (uFogMode == 1) 
						{
							fogdist = pixelpos.w;
						}
						else 
						{
							fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
						}
						fogfactor = exp2 (fogparm.z * fogdist);
					}
					
					frag *= getLightColor(fogdist, fogfactor);
					
					
					if (lightrange.w > lightrange.z)
					{
						vec4 addlight = vec4(0.0,0.0,0.0,0.0);
					
						//
						// additive lights - these can be done after the alpha test.
						//
						for(int i=lightrange.z; i<lightrange.w; i+=2)
						{
							vec4 lightpos = Parameters[i];
							vec4 lightcolor = Parameters[i+1];
							
							lightcolor.rgb *= max(lightpos.w - distance(pixelpos.xyz, lightpos.xyz),0.0) / lightpos.w;
							addlight.rgb += lightcolor.rgb;
						}
						frag.rgb = clamp(frag.rgb + desaturate(addlight).rgb, 0.0, 1.0);
					}

					//
					// colored fog
					//
					if (fogenabled < 0) 
					{
						frag = applyFog(frag, fogfactor);
					}
					break;
				}
				
				case 1:
				{
					float gray = (frag.r * 0.3 + frag.g * 0.56 + frag.b * 0.14);	
					vec4 cm = uFixedColormapStart + gray * uFixedColormapRange;
					frag = vec4(clamp(cm.rgb, 0.0, 1.0), frag.a*gl_Color.a);
					break;
				}
				
				case 2:
				{
					frag = frag * uFixedColormapStart;
					frag.a *= gl_Color.a;
					break;
				}
			}
			break;
		
		case SPECMODE_INFRARED:
		{
			float gray = 1.0 - (frag.r * 0.3 + frag.g * 0.56 + frag.b * 0.14);	
			frag = vec4(gray, gray, gray, frag.a*gl_Color.a) * uFixedColormapStart;
			break;
		}
		
		case SPECMODE_FOGLAYER:
		{
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
			
			frag = vec4(fogcolor.rgb, (1.0 - fogfactor) * frag.a * 0.75 * gl_Color.a);
			break;
		}
	}
	gl_FragColor = frag;
}

