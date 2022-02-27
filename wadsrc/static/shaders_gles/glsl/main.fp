
varying vec4 vTexCoord;
varying vec4 vColor;
varying vec4 pixelpos;
varying vec3 glowdist;
varying vec3 gradientdist;
varying vec4 vWorldNormal;
varying vec4 vEyeNormal;

#ifdef NO_CLIPDISTANCE_SUPPORT
varying vec4 ClipDistanceA;
varying vec4 ClipDistanceB;
#endif


struct Material
{
	vec4 Base;
	vec4 Bright;
	vec4 Glow;
	vec3 Normal;
	vec3 Specular;
	float Glossiness;
	float SpecularLevel;
};

vec4 Process(vec4 color);
vec4 ProcessTexel();
Material ProcessMaterial(); // note that this is deprecated. Use SetupMaterial!
void SetupMaterial(inout Material mat);
vec4 ProcessLight(Material mat, vec4 color);
vec3 ProcessMaterialLight(Material material, vec3 color);
vec2 GetTexCoord();

// These get Or'ed into uTextureMode because it only uses its 3 lowermost bits.
//const int TEXF_Brightmap = 0x10000;
//const int TEXF_Detailmap = 0x20000;
//const int TEXF_Glowmap = 0x40000;


//===========================================================================
//
// Color to grayscale
//
//===========================================================================

float grayscale(vec4 color)
{
	return dot(color.rgb, vec3(0.3, 0.56, 0.14));
}

//===========================================================================
//
// Desaturate a color
//
//===========================================================================

vec4 dodesaturate(vec4 texel, float factor)
{
	if (factor != 0.0)
	{
		float gray = grayscale(texel);
		return mix (texel, vec4(gray,gray,gray,texel.a), factor);
	}
	else
	{
		return texel;
	}
}

//===========================================================================
//
// Desaturate a color
//
//===========================================================================

vec4 desaturate(vec4 texel)
{
#if (DEF_DO_DESATURATE == 1)
	return dodesaturate(texel, uDesaturationFactor);
#else
	return texel;
#endif
}

//===========================================================================
//
// Texture tinting code originally from JFDuke but with a few more options
//
//===========================================================================

const int Tex_Blend_Alpha = 1;
const int Tex_Blend_Screen = 2;
const int Tex_Blend_Overlay = 3;
const int Tex_Blend_Hardlight = 4;
 
 vec4 ApplyTextureManipulation(vec4 texel)
 {
	// Step 1: desaturate according to the material's desaturation factor. 
	texel = dodesaturate(texel, uTextureModulateColor.a);
	
	// Step 2: Invert if requested // TODO FIX
	//if ((blendflags & 8) != 0)
	//{
	//	texel.rgb = vec3(1.0 - texel.r, 1.0 - texel.g, 1.0 - texel.b);
	//}
	
	// Step 3: Apply additive color
	texel.rgb += uTextureAddColor.rgb;
	
	// Step 4: Colorization, including gradient if set.
	texel.rgb *= uTextureModulateColor.rgb;
	
	// Before applying the blend the value needs to be clamped to [0..1] range.
	texel.rgb = clamp(texel.rgb, 0.0, 1.0);
	
	// Step 5: Apply a blend. This may just be a translucent overlay or one of the blend modes present in current Build engines.
#if (DEF_BLEND_FLAGS != 0)
	
	vec3 tcol = texel.rgb * 255.0;	// * 255.0 to make it easier to reuse the integer math.
	vec4 tint = uTextureBlendColor * 255.0;

#if (DEF_BLEND_FLAGS == 1)
	
	tcol.b = tcol.b * (1.0 - uTextureBlendColor.a) + tint.b * uTextureBlendColor.a;
	tcol.g = tcol.g * (1.0 - uTextureBlendColor.a) + tint.g * uTextureBlendColor.a;
	tcol.r = tcol.r * (1.0 - uTextureBlendColor.a) + tint.r * uTextureBlendColor.a;

#elif (DEF_BLEND_FLAGS == 2) // Tex_Blend_Screen:
	tcol.b = 255.0 - (((255.0 - tcol.b) * (255.0 - tint.r)) / 256.0);
	tcol.g = 255.0 - (((255.0 - tcol.g) * (255.0 - tint.g)) / 256.0);
	tcol.r = 255.0 - (((255.0 - tcol.r) * (255.0 - tint.b)) / 256.0);

#elif (DEF_BLEND_FLAGS == 3) // Tex_Blend_Overlay:
	
	tcol.b = tcol.b < 128.0? (tcol.b * tint.b) / 128.0 : 255.0 - (((255.0 - tcol.b) * (255.0 - tint.b)) / 128.0);
	tcol.g = tcol.g < 128.0? (tcol.g * tint.g) / 128.0 : 255.0 - (((255.0 - tcol.g) * (255.0 - tint.g)) / 128.0);
	tcol.r = tcol.r < 128.0? (tcol.r * tint.r) / 128.0 : 255.0 - (((255.0 - tcol.r) * (255.0 - tint.r)) / 128.0);

#elif (DEF_BLEND_FLAGS == 4) // Tex_Blend_Hardlight:

	tcol.b = tint.b < 128.0 ? (tcol.b * tint.b) / 128.0 : 255.0 - (((255.0 - tcol.b) * (255.0 - tint.b)) / 128.0);
	tcol.g = tint.g < 128.0 ? (tcol.g * tint.g) / 128.0 : 255.0 - (((255.0 - tcol.g) * (255.0 - tint.g)) / 128.0);
	tcol.r = tint.r < 128.0 ? (tcol.r * tint.r) / 128.0 : 255.0 - (((255.0 - tcol.r) * (255.0 - tint.r)) / 128.0);

#endif
	
	texel.rgb = tcol / 255.0;
	
#endif

	return texel;
}

//===========================================================================
//
// This function is common for all (non-special-effect) fragment shaders
//
//===========================================================================

vec4 getTexel(vec2 st)
{
	vec4 texel = texture2D(tex, st);
	
#if (DEF_TEXTURE_MODE == 1)

	texel.rgb = vec3(1.0,1.0,1.0);
	
#elif (DEF_TEXTURE_MODE == 2)// TM_OPAQUE
	
	texel.a = 1.0;
				
#elif (DEF_TEXTURE_MODE == 3)// TM_INVERSE
	
	texel = vec4(1.0-texel.r, 1.0-texel.b, 1.0-texel.g, texel.a);

#elif (DEF_TEXTURE_MODE == 4)// TM_ALPHATEXTURE

	float gray = grayscale(texel);
	texel = vec4(1.0, 1.0, 1.0, gray*texel.a);
			
#elif (DEF_TEXTURE_MODE == 5)// TM_CLAMPY
			
	if (st.t < 0.0 || st.t > 1.0)
	{
		texel.a = 0.0;
	}
			
#elif (DEF_TEXTURE_MODE == 6)// TM_OPAQUEINVERSE

	texel = vec4(1.0-texel.r, 1.0-texel.b, 1.0-texel.g, 1.0);

			
#elif (DEF_TEXTURE_MODE == 7)//TM_FOGLAYER 
	
	return texel;
	
#endif
	
	// Apply the texture modification colors.
#if (DEF_BLEND_FLAGS != 0)	

	// only apply the texture manipulation if it contains something.
	texel = ApplyTextureManipulation(texel, DEF_BLEND_FLAGS);

#endif

	// Apply the Doom64 style material colors on top of everything from the texture modification settings.
	// This may be a bit redundant in terms of features but the data comes from different sources so this is unavoidable.
	
	texel.rgb += uAddColor.rgb;

#if (DEF_USE_OBJECT_COLOR_2 == 1)
	texel *= mix(uObjectColor, uObjectColor2, gradientdist.z);
#else
	texel *= uObjectColor;
#endif

	// Last but not least apply the desaturation from the sector's light.
	return desaturate(texel);
}




//===========================================================================
//
// Doom software lighting equation
//
//===========================================================================

#define DOOMLIGHTFACTOR 232.0

float R_DoomLightingEquation_OLD(float light)
{
	// z is the depth in view space, positive going into the screen
	float z = pixelpos.w;

	
		/* L in the range 0 to 63 */
	float L = light * 63.0/31.0;

	float min_L = clamp(36.0/31.0 - L, 0.0, 1.0);

	// Fix objects getting totally black when close.
	if (z < 0.0001)
		z = 0.0001;

	float scale = 1.0 / z;
	float index = (59.0/31.0 - L) - (scale * DOOMLIGHTFACTOR/31.0 - DOOMLIGHTFACTOR/31.0);

	// Result is the normalized colormap index (0 bright .. 1 dark)
	return clamp(index, min_L, 1.0) / 32.0;
}


//===========================================================================
//
// zdoom colormap equation
//
//===========================================================================
float R_ZDoomColormap(float light, float z)
{
	float L = light * 255.0;
	float vis = min(uGlobVis / z, 24.0 / 32.0);
	float shade = 2.0 - (L + 12.0) / 128.0;
	float lightscale = shade - vis;
	return lightscale * 31.0;
}

//===========================================================================
//
// Doom software lighting equation
//
//===========================================================================
float R_DoomLightingEquation(float light)
{
	// z is the depth in view space, positive going into the screen
	float z;

#if (DEF_FOG_RADIAL == 1)
	z = distance(pixelpos.xyz, uCameraPos.xyz);
#else
	z = pixelpos.w;
#endif

#if (DEF_BUILD_LIGHTING == 1) // gl_lightmode 5: Build software lighting emulation.
	// This is a lot more primitive than Doom's lighting...
	float numShades = float(uPalLightLevels);
	float curshade = (1.0 - light) * (numShades - 1.0);
	float visibility = max(uGlobVis * uLightFactor * abs(z), 0.0);
	float shade = clamp((curshade + visibility), 0.0, numShades - 1.0);
	return clamp(shade * uLightDist, 0.0, 1.0);
#endif

	float colormap = R_ZDoomColormap(light, z); // ONLY Software mode, vanilla not yet working

#if (DEF_BANDED_SW_LIGHTING == 1) 
	colormap = floor(colormap) + 0.5;
#endif

	// Result is the normalized colormap index (0 bright .. 1 dark)
	return clamp(colormap, 0.0, 31.0) / 32.0;
}


float shadowAttenuation(vec4 lightpos, float lightcolorA)
{
	return 1.0;
}


float spotLightAttenuation(vec4 lightpos, vec3 spotdir, float lightCosInnerAngle, float lightCosOuterAngle)
{
	vec3 lightDirection = normalize(lightpos.xyz - pixelpos.xyz);
	float cosDir = dot(lightDirection, spotdir);
	return smoothstep(lightCosOuterAngle, lightCosInnerAngle, cosDir);
}

vec3 ApplyNormalMap(vec2 texcoord)
{
	return normalize(vWorldNormal.xyz);
}

//===========================================================================
//
// Sets the common material properties.
//
//===========================================================================

void SetMaterialProps(inout Material material, vec2 texCoord)
{

#ifdef NPOT_EMULATION

#if (DEF_NPOT_EMULATION == 1)
		float period = floor(texCoord.t / uNpotEmulation.y);
		texCoord.s += uNpotEmulation.x * floor(mod(texCoord.t, uNpotEmulation.y));
		texCoord.t = period + mod(texCoord.t, uNpotEmulation.y);
#endif

#endif

	material.Base = getTexel(texCoord.st);
	material.Normal = ApplyNormalMap(texCoord.st);

	#if (DEF_TEXTURE_FLAGS & 0x1)
		material.Bright = texture2D(brighttexture, texCoord.st);
	#endif

	#if (DEF_TEXTURE_FLAGS & 0x2)
	{
		vec4 Detail = texture2D(detailtexture, texCoord.st * uDetailParms.xy) * uDetailParms.z;
		material.Base *= Detail;
	}
	#endif

	#if (DEF_TEXTURE_FLAGS & 0x4)
	{
		material.Glow = texture2D(glowtexture, texCoord.st);
	}
	#endif

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

vec4 getLightColor(Material material, float fogdist, float fogfactor)
{
	vec4 color = vColor;
	
#if (DEF_USE_U_LIGHT_LEVEL == 1)
	{
		float newlightlevel = 1.0 - R_DoomLightingEquation(uLightLevel);
		color.rgb *= newlightlevel;
	}
#else
	{

		#if (DEF_FOG_ENABLED == 1) && (DEF_FOG_COLOURED == 0)
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
		#endif
	}
#endif	


	//
	// handle glowing walls
	//
#if (DEF_USE_GLOW_TOP_COLOR)	
	if (glowdist.x < uGlowTopColor.a)
	{
		color.rgb += desaturate(uGlowTopColor * (1.0 - glowdist.x / uGlowTopColor.a)).rgb;
	}
#endif


#if (DEF_USE_GLOW_BOTTOM_COLOR)	
	if (glowdist.y < uGlowBottomColor.a)
	{
		color.rgb += desaturate(uGlowBottomColor * (1.0 - glowdist.y / uGlowBottomColor.a)).rgb;
	}
#endif

	color = min(color, 1.0);

	// these cannot be safely applied by the legacy format where the implementation cannot guarantee that the values are set.
#ifndef LEGACY_USER_SHADER
	//
	// apply glow 
	//
	color.rgb = mix(color.rgb, material.Glow.rgb, material.Glow.a);

	//
	// apply brightmaps 
	//
	color.rgb = min(color.rgb + material.Bright.rgb, 1.0);
#endif
	
	//
	// apply other light manipulation by custom shaders, default is a NOP.
	//
	color = ProcessLight(material, color);

	//
	// apply dynamic lights
	//
	return vec4(ProcessMaterialLight(material, color.rgb), material.Base.a * vColor.a);
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

	//if (ClipDistanceA.x < 0.0 || ClipDistanceA.y < 0.0 || ClipDistanceA.z < 0.0 || ClipDistanceA.w < 0.0 || ClipDistanceB.x < 0.0) discard;

#ifndef LEGACY_USER_SHADER
	Material material;
	
	material.Base = vec4(0.0);
	material.Bright = vec4(0.0);
	material.Glow = vec4(0.0);
	material.Normal = vec3(0.0);
	material.Specular = vec3(0.0);
	material.Glossiness = 0.0;
	material.SpecularLevel = 0.0;
	SetupMaterial(material);
#else
	Material material = ProcessMaterial();
#endif
	vec4 frag = material.Base;

#ifndef NO_ALPHATEST
	if (frag.a <= uAlphaThreshold) discard;
#endif

	#if (DEF_FOG_2D == 0)	// check for special 2D 'fog' mode.
	{
		float fogdist = 0.0;
		float fogfactor = 0.0;
		
		//
		// calculate fog factor
		//
		#if (DEF_FOG_ENABLED == 1)
		{
			#if (DEF_FOG_RADIAL == 0)
				fogdist = max(16.0, pixelpos.w);
			#else
				fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
			#endif

			fogfactor = exp2 (uFogDensity * fogdist);
		}
		#endif

		#if (DEF_TEXTURE_MODE != 7)
		{
			frag = getLightColor(material, fogdist, fogfactor);

			//
			// colored fog
			//
			#if (DEF_FOG_ENABLED == 1) && (DEF_FOG_COLOURED == 1)
			{
				frag = applyFog(frag, fogfactor);
			}
			#endif
		}
		#else
		{
			frag = vec4(uFogColor.rgb, (1.0 - fogfactor) * frag.a * 0.75 * vColor.a);
		}
		#endif
	}	
	#else
	{
		#if (DEF_TEXTURE_MODE == 7)
		{
			float gray = grayscale(frag);
			vec4 cm = (uObjectColor + gray * (uAddColor - uObjectColor)) * 2.0;
			frag = vec4(clamp(cm.rgb, 0.0, 1.0), frag.a);
		}		
		#endif
	
		frag = frag * ProcessLight(material, vColor);
		frag.rgb = frag.rgb + uFogColor.rgb;
	}
	#endif  // (DEF_2D_FOG == 0)
	
#if (DEF_USE_COLOR_MAP == 1) // This mostly works but doesn't look great because of the blending.
	{
		frag.rgb = clamp(pow(frag.rgb, vec3(uFixedColormapStart.a)), 0.0, 1.0);
		if (uFixedColormapRange.a == 0.0)
		{
			float gray = (frag.r * 0.3 + frag.g * 0.56 + frag.b * 0.14);	
			vec4 cm = uFixedColormapStart + gray * uFixedColormapRange;
			frag.rgb = clamp(cm.rgb, 0.0, 1.0);
		} 
	}
#endif

	gl_FragColor = frag;

	//gl_FragColor = vec4(0.8, 0.2, 0.5, 1);

}
