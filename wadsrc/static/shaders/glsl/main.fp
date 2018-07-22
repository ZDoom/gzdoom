in vec4 pixelpos;
in vec3 glowdist;

in vec4 vWorldNormal;
in vec4 vEyeNormal;
in vec4 vTexCoord;
in vec4 vColor;

layout(location=0) out vec4 FragColor;
#ifdef GBUFFER_PASS
layout(location=1) out vec4 FragFog;
layout(location=2) out vec4 FragNormal;
#endif

struct Material
{
	vec4 Base;
	vec4 Bright;
	vec3 Normal;
	vec3 Specular;
	float Glossiness;
	float SpecularLevel;
	float Metallic;
	float Roughness;
	float AO;
};

vec4 Process(vec4 color);
vec4 ProcessTexel();
Material ProcessMaterial();
vec4 ProcessLight(Material mat, vec4 color);
vec3 ProcessMaterialLight(Material material, vec3 color);

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

vec4 desaturate(vec4 texel)
{
	if (uDesaturationFactor > 0.0)
	{
		float gray = grayscale(texel);
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
		{
			float gray = grayscale(texel);
			texel = vec4(1.0, 1.0, 1.0, gray*texel.a);
			break;
		}
			
		case 5:	// TM_CLAMPY
			if (st.t < 0.0 || st.t > 1.0)
			{
				texel.a = 0.0;
			}
			break;
			
		case 6: // TM_OPAQUEINVERSE
			texel = vec4(1.0-texel.r, 1.0-texel.b, 1.0-texel.g, 1.0);
			break;
			
		case 7: //TM_FOGLAYER 
			return texel;

	}
	if (uObjectColor2.a == 0.0) texel *= uObjectColor;
	else texel *= mix(uObjectColor, uObjectColor2, glowdist.z);

	return desaturate(texel);
}

//===========================================================================
//
// Doom lighting equation exactly as calculated by zdoom.
//
//===========================================================================
float R_DoomLightingEquation(float light)
{
	// L is the integer light level used in the game
	float L = light * 255.0;

	// z is the depth in view/eye space, positive going into the screen
	float z;
	if ((uPalLightLevels >> 8) == 2)
	{
		z = distance(pixelpos.xyz, uCameraPos.xyz);
	}
	else 
	{
		z = pixelpos.w;
	}

	// The zdoom light equation
	float vis = min(uGlobVis / z, 24.0 / 32.0);
	float shade = 2.0 - (L + 12.0) / 128.0;
	float lightscale;
	if ((uPalLightLevels & 0xff) != 0)
		lightscale = float(-floor(-(shade - vis) * 31.0) - 0.5) / 31.0;
	else
		lightscale = shade - vis;

	// Result is the normalized colormap index (0 bright .. 1 dark)
	return clamp(lightscale, 0.0, 31.0 / 32.0);
}

//===========================================================================
//
// Check if light is in shadow according to its 1D shadow map
//
//===========================================================================

#ifdef SUPPORTS_SHADOWMAPS

float shadowDirToU(vec2 dir)
{
	if (abs(dir.x) > abs(dir.y))
	{
		if (dir.x >= 0.0)
			return dir.y / dir.x * 0.125 + (0.25 + 0.125);
		else
			return dir.y / dir.x * 0.125 + (0.75 + 0.125);
	}
	else
	{
		if (dir.y >= 0.0)
			return dir.x / dir.y * 0.125 + 0.125;
		else
			return dir.x / dir.y * 0.125 + (0.50 + 0.125);
	}
}

float sampleShadowmap(vec2 dir, float v)
{
	float u = shadowDirToU(dir);
	float dist2 = dot(dir, dir);
	return texture(ShadowMap, vec2(u, v)).x > dist2 ? 1.0 : 0.0;
}

float sampleShadowmapLinear(vec2 dir, float v)
{
	float u = shadowDirToU(dir);
	float dist2 = dot(dir, dir);

	vec2 isize = textureSize(ShadowMap, 0);
	vec2 size = vec2(isize);

	vec2 fetchPos = vec2(u, v) * size - vec2(0.5, 0.0);
	if (fetchPos.x < 0.0)
		fetchPos.x += size.x;

	ivec2 ifetchPos = ivec2(fetchPos);
	int y = ifetchPos.y;

	float t = fract(fetchPos.x);
	int x0 = ifetchPos.x;
	int x1 = ifetchPos.x + 1;
	if (x1 == isize.x)
		x1 = 0;

	float depth0 = texelFetch(ShadowMap, ivec2(x0, y), 0).x;
	float depth1 = texelFetch(ShadowMap, ivec2(x1, y), 0).x;
	return mix(step(dist2, depth0), step(dist2, depth1), t);
}

//===========================================================================
//
// Check if light is in shadow using Percentage Closer Filtering (PCF)
//
//===========================================================================

#define PCF_FILTER_STEP_COUNT 3
#define PCF_COUNT (PCF_FILTER_STEP_COUNT * 2 + 1)

// #define USE_LINEAR_SHADOW_FILTER
#define USE_PCF_SHADOW_FILTER 1

float shadowmapAttenuation(vec4 lightpos, float shadowIndex)
{
	if (shadowIndex >= 1024.0)
		return 1.0; // No shadowmap available for this light

	float v = (shadowIndex + 0.5) / 1024.0;

	vec2 ray = pixelpos.xz - lightpos.xz;
	float length = length(ray);
	if (length < 3.0)
		return 1.0;

	vec2 dir = ray / length;

#if defined(USE_LINEAR_SHADOW_FILTER)
	ray -= dir * 6.0; // Shadow acne margin
	return sampleShadowmapLinear(ray, v);
#elif defined(USE_PCF_SHADOW_FILTER)
	ray -= dir * 2.0; // Shadow acne margin
	dir = dir * min(length / 50.0, 1.0); // avoid sampling behind light

	vec2 normal = vec2(-dir.y, dir.x);
	vec2 bias = dir * 10.0;

	float sum = 0.0;
	for (float x = -PCF_FILTER_STEP_COUNT; x <= PCF_FILTER_STEP_COUNT; x++)
	{
		sum += sampleShadowmap(ray + normal * x - bias * abs(x), v);
	}
	return sum / PCF_COUNT;
#else // nearest shadow filter
	ray -= dir * 6.0; // Shadow acne margin
	return sampleShadowmap(ray, v);
#endif
}

float shadowAttenuation(vec4 lightpos, float lightcolorA)
{
	float shadowIndex = abs(lightcolorA) - 1.0;
	return shadowmapAttenuation(lightpos, shadowIndex);
}

#else

float shadowAttenuation(vec4 lightpos, float lightcolorA)
{
	return 1.0;
}

#endif

float spotLightAttenuation(vec4 lightpos, vec3 spotdir, float lightCosInnerAngle, float lightCosOuterAngle)
{
	vec3 lightDirection = normalize(lightpos.xyz - pixelpos.xyz);
	float cosDir = dot(lightDirection, spotdir);
	return smoothstep(lightCosOuterAngle, lightCosInnerAngle, cosDir);
}

//===========================================================================
//
// Adjust normal vector according to the normal map
//
//===========================================================================

#if defined(NORMALMAP)
mat3 cotangent_frame(vec3 n, vec3 p, vec2 uv)
{
	// get edge vectors of the pixel triangle
	vec3 dp1 = dFdx(p);
	vec3 dp2 = dFdy(p);
	vec2 duv1 = dFdx(uv);
	vec2 duv2 = dFdy(uv);

	// solve the linear system
	vec3 dp2perp = cross(n, dp2); // cross(dp2, n);
	vec3 dp1perp = cross(dp1, n); // cross(n, dp1);
	vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;

	// construct a scale-invariant frame
	float invmax = inversesqrt(max(dot(t,t), dot(b,b)));
	return mat3(t * invmax, b * invmax, n);
}

vec3 ApplyNormalMap(vec2 texcoord)
{
	#define WITH_NORMALMAP_UNSIGNED
	#define WITH_NORMALMAP_GREEN_UP
	//#define WITH_NORMALMAP_2CHANNEL

	vec3 interpolatedNormal = normalize(vWorldNormal.xyz);

	vec3 map = texture(normaltexture, texcoord).xyz;
	#if defined(WITH_NORMALMAP_UNSIGNED)
	map = map * 255./127. - 128./127.; // Math so "odd" because 0.5 cannot be precisely described in an unsigned format
	#endif
	#if defined(WITH_NORMALMAP_2CHANNEL)
	map.z = sqrt(1 - dot(map.xy, map.xy));
	#endif
	#if defined(WITH_NORMALMAP_GREEN_UP)
	map.y = -map.y;
	#endif

	mat3 tbn = cotangent_frame(interpolatedNormal, pixelpos.xyz, vTexCoord.st);
	vec3 bumpedNormal = normalize(tbn * map);
	return bumpedNormal;
}
#else
vec3 ApplyNormalMap(vec2 texcoord)
{
	return normalize(vWorldNormal.xyz);
}
#endif

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
// The color of the fragment if it is fully occluded by ambient lighting
//
//===========================================================================

vec3 AmbientOcclusionColor()
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
			
	return mix(uFogColor.rgb, vec3(0.0), fogfactor);
}

//===========================================================================
//
// Main shader routine
//
//===========================================================================

void main()
{
	Material material = ProcessMaterial();
	vec4 frag = material.Base;
	
#ifndef NO_ALPHATEST
	if (frag.a <= uAlphaThreshold) discard;
#endif

	if (uFogEnabled != -3)	// check for special 2D 'fog' mode.
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
		
		if (uTextureMode != 7)
		{
			frag = getLightColor(material, fogdist, fogfactor);
			//
			// colored fog
			//
			if (uFogEnabled < 0) 
			{
				frag = applyFog(frag, fogfactor);
			}
		}
		else
		{
			frag = vec4(uFogColor.rgb, (1.0 - fogfactor) * frag.a * 0.75 * vColor.a);
		}
	}
	else // simple 2D (uses the fog color to add a color overlay)
	{
		if (uTextureMode == 7)
		{
			float gray = grayscale(frag);
			vec4 cm = (uObjectColor + gray * (uObjectColor2 - uObjectColor)) * 2;
			frag = vec4(clamp(cm.rgb, 0.0, 1.0), frag.a);
		}
			frag = frag * ProcessLight(material, vColor);
		frag.rgb = frag.rgb + uFogColor.rgb;
	}
	FragColor = frag;
#ifdef GBUFFER_PASS
	FragFog = vec4(AmbientOcclusionColor(), 1.0);
	FragNormal = vec4(vEyeNormal.xyz * 0.5 + 0.5, 1.0);
#endif
}
