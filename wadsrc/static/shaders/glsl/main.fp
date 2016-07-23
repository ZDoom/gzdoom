in vec4 pixelpos;
in vec2 glowdist;

in vec4 vTexCoord;
in vec4 vColor;

out vec4 FragColor;

#ifdef SHADER_STORAGE_LIGHTS
	layout(std430, binding = 1) buffer LightBufferSSO
	{
		vec4 lights[];
	};
#elif defined NUM_UBO_LIGHTS
	/*layout(std140)*/ uniform LightBufferUBO
	{
		vec4 lights[NUM_UBO_LIGHTS];
	};
#endif


uniform sampler2D tex;

vec4 Process(vec4 color);
vec4 ProcessTexel();
vec4 ProcessLight(vec4 color);


//===========================================================================
//
// Desaturate a color
//
//===========================================================================

vec4 desaturate(vec4 texel)
{
	if (uDesaturationFactor > 0.0)
	{
		float gray = (texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14);	
		return mix (texel, vec4(gray,gray,gray,texel.a), uDesaturationFactor);
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

#ifdef GLSL12_COMPATIBLE
vec4 getTexel(vec2 st)
{
	vec4 texel = texture(tex, st);
	
	//
	// Apply texture modes
	//
	if (uTextureMode != 0)
	{
		if (uTextureMode == 1) texel.rgb = vec3(1.0,1.0,1.0);
		else if (uTextureMode == 2) texel.a = 1.0;
		else if (uTextureMode == 3) texel = vec4(1.0-texel.r, 1.0-texel.b, 1.0-texel.g, texel.a);
		else if (uTextureMode == 4) texel = vec4(1.0, 1.0, 1.0, texel.r*texel.a);
		else if (uTextureMode == 5)
		{
			if (st.t < 0.0 || st.t > 1.0)
			{
				texel.a = 0.0;
			}
		}
	}
	texel *= uObjectColor;
	return desaturate(texel);
}
#else
vec4 getTexel(vec2 st)
{
	vec4 texel = texture(tex, st);
	
	//
	// Apply texture modes
	//
	switch (uTextureMode)
	{
		case 1:	// TM_MASK
			texel.rgb = vec3(1.0,1.0,1.0);
			break;
			
		case 2:	// TM_OPAQUE
			texel.a = 1.0;
			break;
			
		case 3:	// TM_INVERSE
			texel = vec4(1.0-texel.r, 1.0-texel.b, 1.0-texel.g, texel.a);
			break;
			
		case 4:	// TM_REDTOALPHA
			texel = vec4(1.0, 1.0, 1.0, texel.r*texel.a);
			break;
			
		case 5:	// TM_CLAMPY
			if (st.t < 0.0 || st.t > 1.0)
			{
				texel.a = 0.0;
			}
			break;
	}
	texel *= uObjectColor;

	return desaturate(texel);
}
#endif

//===========================================================================
//
// Doom lighting equation exactly as calculated by zdoom.
//
//===========================================================================
float R_DoomLightingEquation(float light)
{
	// Calculated from r_visibility. It differs between walls, floor and sprites.
	//
	// Wall: globVis = r_WallVisibility
	// Floor: r_FloorVisibility / abs(plane.Zat0 - ViewPos.Z)
	// Sprite: same as wall
	// All are calculated in R_SetVisibility and seem to be decided by the
	// aspect ratio amongst other things.
	//
	// 1706 is the value for walls on 1080p 16:9 displays.
	float globVis = 1706.0;

	/* L is the integer light level used in the game */
	float L = light * 255.0;

	/* z is the depth in view/eye space, positive going into the screen */
	float z = pixelpos.w;

	/* The zdoom light equation */
	float vis = globVis / z;
	float shade = 64.0 - (L + 12.0) * 32.0/128.0;
	float lightscale = clamp((shade - min(24.0, vis)) / 32.0, 0.0, 31.0/32.0);

	// Result is the normalized colormap index (0 bright .. 1 dark)
	return lightscale;
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
// This is making this a bit more complicated than it needs to
// because we can't just desaturate the final fragment color.
//
//===========================================================================

vec4 getLightColor(float fogdist, float fogfactor)
{
	vec4 color = vColor;
	
	if (uLightLevel >= 0.0)
	{
		float newlightlevel = 1.0 - R_DoomLightingEquation(uLightLevel);
		color.rgb *= newlightlevel;
	}
	else if (uFogEnabled > 0)
	{
		// brightening around the player for light mode 2
		if (fogdist < uLightDist)
		{
			color.rgb *= uLightFactor - (fogdist / uLightDist) * (uLightFactor - 1.0);
		}
		
		//
		// apply light diminishing through fog equation
		//
		color.rgb = mix(vec3(0.0, 0.0, 0.0), color.rgb, fogfactor);
	}
	
	//
	// handle glowing walls
	//
	if (uGlowTopColor.a > 0.0 && glowdist.x < uGlowTopColor.a)
	{
		color.rgb += desaturate(uGlowTopColor * (1.0 - glowdist.x / uGlowTopColor.a)).rgb;
	}
	if (uGlowBottomColor.a > 0.0 && glowdist.y < uGlowBottomColor.a)
	{
		color.rgb += desaturate(uGlowBottomColor * (1.0 - glowdist.y / uGlowBottomColor.a)).rgb;
	}
	color = min(color, 1.0);

	//
	// apply brightmaps (or other light manipulation by custom shaders.
	//
	color = ProcessLight(color);

	//
	// apply dynamic lights (except additive)
	//
	
	vec4 dynlight = uDynLightColor;

#if defined NUM_UBO_LIGHTS || defined SHADER_STORAGE_LIGHTS
	if (uLightIndex >= 0)
	{
		ivec4 lightRange = ivec4(lights[uLightIndex]) + ivec4(uLightIndex + 1);
		if (lightRange.z > lightRange.x)
		{
			//
			// modulated lights
			//
			for(int i=lightRange.x; i<lightRange.y; i+=2)
			{
				vec4 lightpos = lights[i];
				vec4 lightcolor = lights[i+1];
				
				lightcolor.rgb *= max(lightpos.w - distance(pixelpos.xyz, lightpos.xyz),0.0) / lightpos.w;
				dynlight.rgb += lightcolor.rgb;
			}
			//
			// subtractive lights
			//
			for(int i=lightRange.y; i<lightRange.z; i+=2)
			{
				vec4 lightpos = lights[i];
				vec4 lightcolor = lights[i+1];
				
				lightcolor.rgb *= max(lightpos.w - distance(pixelpos.xyz, lightpos.xyz),0.0) / lightpos.w;
				dynlight.rgb -= lightcolor.rgb;
			}
		}
	}
#endif
	color.rgb = clamp(color.rgb + desaturate(dynlight).rgb, 0.0, 1.4);
	
	// prevent any unintentional messing around with the alpha.
	return vec4(color.rgb, vColor.a);
}

//===========================================================================
//
// Applies colored fog
//
//===========================================================================

vec4 applyFog(vec4 frag, float fogfactor)
{
	return vec4(mix(uFogColor.rgb, frag.rgb, fogfactor), frag.a);
}


//===========================================================================
//
// Main shader routine
//
//===========================================================================

void main()
{
	vec4 frag = ProcessTexel();
	
#ifndef NO_ALPHATEST
	if (frag.a <= uAlphaThreshold) discard;
#endif

	switch (uFixedColormap)
	{
		case 0:
		{
			float fogdist = 0.0;
			float fogfactor = 0.0;
			

			
			//
			// calculate fog factor
			//
			if (uFogEnabled != 0)
			{
				if (uFogEnabled == 1 || uFogEnabled == -1) 
				{
					fogdist = pixelpos.w;
				}
				else 
				{
					fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
				}
				fogfactor = exp2 (uFogDensity * fogdist);
			}
			
			
			frag *= getLightColor(fogdist, fogfactor);
			
#if defined NUM_UBO_LIGHTS || defined SHADER_STORAGE_LIGHTS
			if (uLightIndex >= 0)
			{
				ivec4 lightRange = ivec4(lights[uLightIndex]) + ivec4(uLightIndex + 1);
				if (lightRange.w > lightRange.z)
				{
					vec4 addlight = vec4(0.0,0.0,0.0,0.0);
				
					//
					// additive lights - these can be done after the alpha test.
					//
					for(int i=lightRange.z; i<lightRange.w; i+=2)
					{
						vec4 lightpos = lights[i];
						vec4 lightcolor = lights[i+1];
						
						lightcolor.rgb *= max(lightpos.w - distance(pixelpos.xyz, lightpos.xyz),0.0) / lightpos.w;
						addlight.rgb += lightcolor.rgb;
					}
					frag.rgb = clamp(frag.rgb + desaturate(addlight).rgb, 0.0, 1.0);
				}
			}
#endif

			//
			// colored fog
			//
			if (uFogEnabled < 0) 
			{
				frag = applyFog(frag, fogfactor);
			}
			
			break;
		}
		
		case 1:
		{
			float gray = (frag.r * 0.3 + frag.g * 0.56 + frag.b * 0.14);	
			vec4 cm = uFixedColormapStart + gray * uFixedColormapRange;
			frag = vec4(clamp(cm.rgb, 0.0, 1.0), frag.a*vColor.a);
			break;
		}
		
		case 2:
		{
			frag = vColor * frag * uFixedColormapStart;
			break;
		}

		case 3:
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
			
			frag = vec4(uFogColor.rgb, (1.0 - fogfactor) * frag.a * 0.75 * vColor.a);
			break;
		}
	}
	FragColor = frag;
}

