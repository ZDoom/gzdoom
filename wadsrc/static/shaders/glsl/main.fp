in vec4 pixelpos;
in vec3 glowdist;

in vec4 vWorldNormal;
in vec4 vEyeNormal;
in vec4 vTexCoord;
in vec4 vColor;

out vec4 FragColor;
#ifdef GBUFFER_PASS
out vec4 FragFog;
out vec4 FragNormal;
#endif

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
uniform sampler2D ShadowMap;

#if defined(SPECULAR)
uniform sampler2D texture2;
uniform sampler2D texture3;
#define normaltexture texture2
#define speculartexture texture3
#elif defined(PBR)
uniform sampler2D texture2;
uniform sampler2D texture3;
uniform sampler2D texture4;
uniform sampler2D texture5;
#define normaltexture texture2
#define metallictexture texture3
#define roughnesstexture texture4
#define aotexture texture5
#endif

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

#endif

//===========================================================================
//
// Standard lambertian diffuse light calculation
//
//===========================================================================

float diffuseContribution(vec3 lightDirection, vec3 normal)
{
	return max(dot(normal, lightDirection), 0.0f);
}

//===========================================================================
//
// Blinn specular light calculation
//
//===========================================================================

float blinnSpecularContribution(float diffuseContribution, vec3 lightDirection, vec3 faceNormal, float glossiness, float specularLevel)
{
	if (diffuseContribution > 0.0f)
	{
		vec3 viewDir = normalize(uCameraPos.xyz - pixelpos.xyz);
		vec3 halfDir = normalize(lightDirection + viewDir);
		float specAngle = max(dot(halfDir, faceNormal), 0.0f);
		float phExp = glossiness * 4.0f;
		return specularLevel * pow(specAngle, phExp);
	}
	else
	{
		return 0.0f;
	}
}

//===========================================================================
//
// Adjust normal vector according to the normal map
//
//===========================================================================

#if defined(SPECULAR) || defined(PBR)
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

vec3 ApplyNormalMap()
{
	#define WITH_NORMALMAP_UNSIGNED
	#define WITH_NORMALMAP_GREEN_UP
	//#define WITH_NORMALMAP_2CHANNEL

	vec3 interpolatedNormal = normalize(vWorldNormal.xyz);

	vec3 map = texture(normaltexture, vTexCoord.st).xyz;
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
vec3 ApplyNormalMap()
{
	return normalize(vWorldNormal.xyz);
}
#endif

//===========================================================================
//
// Calculates the brightness of a dynamic point light
//
//===========================================================================

vec2 pointLightAttenuation(vec4 lightpos, float lightcolorA)
{
	float attenuation = max(lightpos.w - distance(pixelpos.xyz, lightpos.xyz),0.0) / lightpos.w;
	if (attenuation == 0.0) return vec2(0.0);
#ifdef SUPPORTS_SHADOWMAPS
	float shadowIndex = abs(lightcolorA) - 1.0;
	attenuation *= shadowmapAttenuation(lightpos, shadowIndex);
#endif
	if (lightcolorA >= 0.0) // Sign bit is the attenuated light flag
	{
		return vec2(attenuation, 0.0);
	}
	else
	{
		vec3 lightDirection = normalize(lightpos.xyz - pixelpos.xyz);
		vec3 pixelnormal = ApplyNormalMap();
		float diffuseAmount = diffuseContribution(lightDirection, pixelnormal);

#if defined(SPECULAR)
		float specularAmount = blinnSpecularContribution(diffuseAmount, lightDirection, pixelnormal, uSpecularMaterial.x, uSpecularMaterial.y);
		return vec2(diffuseAmount, specularAmount) * attenuation;
#else
		return vec2(attenuation * diffuseAmount, 0.0);
#endif
	}
}

float spotLightAttenuation(vec4 lightpos, vec3 spotdir, float lightCosInnerAngle, float lightCosOuterAngle)
{
	vec3 lightDirection = normalize(lightpos.xyz - pixelpos.xyz);
	float cosDir = dot(lightDirection, spotdir);
	return smoothstep(lightCosOuterAngle, lightCosInnerAngle, cosDir);
}

#if defined(PBR)

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 applyLight(vec3 albedo, vec3 ambientLight)
{
	vec3 worldpos = pixelpos.xyz;

	albedo = pow(albedo, vec3(2.2)); // sRGB to linear
	ambientLight = pow(ambientLight, vec3(2.2));

	vec3 normal = ApplyNormalMap();
	float metallic = texture(metallictexture, vTexCoord.st).r;
	float roughness = texture(roughnesstexture, vTexCoord.st).r;
	float ao = texture(aotexture, vTexCoord.st).r;

	vec3 N = normalize(normal);
	vec3 V = normalize(uCameraPos.xyz - worldpos);

	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	vec3 Lo = uDynLightColor.rgb;

#if defined NUM_UBO_LIGHTS || defined SHADER_STORAGE_LIGHTS
	if (uLightIndex >= 0)
	{
		ivec4 lightRange = ivec4(lights[uLightIndex]) + ivec4(uLightIndex + 1);
		if (lightRange.z > lightRange.x)
		{
			//
			// modulated lights
			//
			for(int i=lightRange.x; i<lightRange.y; i+=4)
			{
				vec4 lightpos = lights[i];
				vec4 lightcolor = lights[i+1];
				vec4 lightspot1 = lights[i+2];
				vec4 lightspot2 = lights[i+3];

				vec3 L = normalize(lightpos.xyz - worldpos);
				vec3 H = normalize(V + L);
				//float distance = length(lightpos.xyz - worldpos);
				//float attenuation = 1.0 / (distance * distance);
				float attenuation = pointLightAttenuation(lightpos, lightcolor.a).x;
				if (lightspot1.w == 1.0)
					attenuation *= spotLightAttenuation(lightpos, lightspot1.xyz, lightspot2.x, lightspot2.y);

				vec3 radiance = lightcolor.rgb * attenuation;

				// cook-torrance brdf
				float NDF = DistributionGGX(N, H, roughness);
				float G = GeometrySmith(N, V, L, roughness);
				vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

				vec3 kS = F;
				vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

				vec3 nominator = NDF * G * F;
				float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
				vec3 specular = nominator / max(denominator, 0.001);

				float NdotL = max(dot(N, L), 0.0);
				Lo += (kD * albedo / PI + specular) * radiance * NdotL;
			}
			//
			// subtractive lights
			//
			for(int i=lightRange.y; i<lightRange.z; i+=4)
			{
				vec4 lightpos = lights[i];
				vec4 lightcolor = lights[i+1];
				vec4 lightspot1 = lights[i+2];
				vec4 lightspot2 = lights[i+3];
				
				vec3 L = normalize(lightpos.xyz - worldpos);
				vec3 H = normalize(V + L);
				//float distance = length(lightpos.xyz - worldpos);
				//float attenuation = 1.0 / (distance * distance);
				float attenuation = pointLightAttenuation(lightpos, lightcolor.a).x;
				if (lightspot1.w == 1.0)
					attenuation *= spotLightAttenuation(lightpos, lightspot1.xyz, lightspot2.x, lightspot2.y);

				vec3 radiance = lightcolor.rgb * attenuation;

				// cook-torrance brdf
				float NDF = DistributionGGX(N, H, roughness);
				float G = GeometrySmith(N, V, L, roughness);
				vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

				vec3 kS = F;
				vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

				vec3 nominator = NDF * G * F;
				float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
				vec3 specular = nominator / max(denominator, 0.001);

				float NdotL = max(dot(N, L), 0.0);
				Lo -= (kD * albedo / PI + specular) * radiance * NdotL;
			}
		}
	}
#endif

	// Pretend we sampled the sector light level from an irradiance map

	vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

	vec3 kS = F;
	vec3 kD = 1.0 - kS;

	vec3 irradiance = ambientLight; // texture(irradianceMap, N).rgb
	vec3 diffuse = irradiance * albedo;

	//kD *= 1.0 - metallic;
	//const float MAX_REFLECTION_LOD = 4.0;
	//vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;
	//vec2 envBRDF = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	//vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

	//vec3 ambient = (kD * diffuse + specular) * ao;
	vec3 ambient = (kD * diffuse) * ao;

	vec3 color = ambient + Lo;

	// Tonemap (reinhard) and apply sRGB gamma
	//color = color / (color + vec3(1.0));
	return pow(color, vec3(1.0 / 2.2));
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

vec4 getLightColor(vec4 material, vec4 materialSpec, float fogdist, float fogfactor)
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

#if defined(PBR)
	return vec4(applyLight(material.rgb, color.rgb), color.a * vColor.a);
#else
	//
	// apply dynamic lights (except additive)
	//
	
	vec4 dynlight = uDynLightColor;
	vec4 specular = vec4(0.0, 0.0, 0.0, 1.0);

#if defined NUM_UBO_LIGHTS || defined SHADER_STORAGE_LIGHTS
	if (uLightIndex >= 0)
	{
		ivec4 lightRange = ivec4(lights[uLightIndex]) + ivec4(uLightIndex + 1);
		if (lightRange.z > lightRange.x)
		{
			//
			// modulated lights
			//
			for(int i=lightRange.x; i<lightRange.y; i+=4)
			{
				vec4 lightpos = lights[i];
				vec4 lightcolor = lights[i+1];
				vec4 lightspot1 = lights[i+2];
				vec4 lightspot2 = lights[i+3];
				
				vec2 attenuation = pointLightAttenuation(lightpos, lightcolor.a);
				if (lightspot1.w == 1.0)
					attenuation.xy *= spotLightAttenuation(lightpos, lightspot1.xyz, lightspot2.x, lightspot2.y);
				dynlight.rgb += lightcolor.rgb * attenuation.x;
				specular.rgb += lightcolor.rgb * attenuation.y;
			}
			//
			// subtractive lights
			//
			for(int i=lightRange.y; i<lightRange.z; i+=4)
			{
				vec4 lightpos = lights[i];
				vec4 lightcolor = lights[i+1];
				vec4 lightspot1 = lights[i+2];
				vec4 lightspot2 = lights[i+3];
				
				vec2 attenuation = pointLightAttenuation(lightpos, lightcolor.a);
				if (lightspot1.w == 1.0)
					attenuation.xy *= spotLightAttenuation(lightpos, lightspot1.xyz, lightspot2.x, lightspot2.y);
				dynlight.rgb -= lightcolor.rgb * attenuation.x;
				specular.rgb -= lightcolor.rgb * attenuation.y;
			}
		}
	}
#endif
	color.rgb = clamp(color.rgb + desaturate(dynlight).rgb, 0.0, 1.4);
	specular.rgb = clamp(specular.rgb + desaturate(specular).rgb, 0.0, 1.4);

	// prevent any unintentional messing around with the alpha.
	return vec4(material.rgb * color.rgb + materialSpec.rgb * specular.rgb, material.a * vColor.a);
#endif
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
			
#if defined(SPECULAR)
			vec4 materialSpec = texture(speculartexture, vTexCoord.st);
#else
			vec4 materialSpec = vec4(0.0);
#endif

			frag = getLightColor(frag, materialSpec, fogdist, fogfactor);
			
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
					for(int i=lightRange.z; i<lightRange.w; i+=4)
					{
						vec4 lightpos = lights[i];
						vec4 lightcolor = lights[i+1];
						vec4 lightspot1 = lights[i+2];
						vec4 lightspot2 = lights[i+3];
						
						float attenuation = pointLightAttenuation(lightpos, lightcolor.a).x;
						if (lightspot1.w == 1.0)
							attenuation *= spotLightAttenuation(lightpos, lightspot1.xyz, lightspot2.x, lightspot2.y);
						lightcolor.rgb *= attenuation;
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
#ifdef GBUFFER_PASS
	FragFog = vec4(AmbientOcclusionColor(), 1.0);
	FragNormal = vec4(vEyeNormal.xyz * 0.5 + 0.5, 1.0);
#endif
}

