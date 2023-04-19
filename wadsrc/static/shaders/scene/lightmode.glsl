
#include "shaders/scene/lightmode_default.glsl"
#include "shaders/scene/lightmode_software.glsl"
#include "shaders/scene/lightmode_vanilla.glsl"
#include "shaders/scene/lightmode_build.glsl"
#include "shaders/scene/material.glsl"

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

vec4 getLightColor(Material material)
{
	#if defined(LIGHTMODE_DEFAULT)
		vec4 color = Lightmode_Default();
	#elif defined(LIGHTMODE_SOFTWARE)
		vec4 color = Lightmode_Software();
	#elif defined(LIGHTMODE_VANILLA)
		vec4 color = Lightmode_Vanilla();
	#elif defined(LIGHTMODE_BUILD)
		vec4 color = Lightmode_Build();
	#endif

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

	// these cannot be safely applied by the legacy format where the implementation cannot guarantee that the values are set.
#if !defined LEGACY_USER_SHADER && !defined NO_LAYERS
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
	// apply lightmaps
	//
	if (vLightmap.z >= 0.0)
	{
		color.rgb += texture(LightMap, vLightmap).rgb;
	}

	//
	// apply dynamic lights
	//
	vec4 frag = vec4(ProcessMaterialLight(material, color.rgb), material.Base.a * vColor.a);

	//
	// colored fog
	//
	#if defined(FOG_AFTER_LIGHTS)

		// calculate fog factor
		#if defined(FOG_RADIAL)
			float fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
		#else
			float fogdist = max(16.0, pixelpos.w);
		#endif
		float fogfactor = exp2 (uFogDensity * fogdist);

		frag = vec4(mix(uFogColor.rgb, frag.rgb, fogfactor), frag.a);

	#endif

	return frag;
}

// The color of the fragment if it is fully occluded by ambient lighting

vec3 AmbientOcclusionColor()
{
	// calculate fog factor
	#if defined(FOG_RADIAL)
		float fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
	#else
		float fogdist = max(16.0, pixelpos.w);
	#endif
	float fogfactor = exp2 (uFogDensity * fogdist);

	return mix(uFogColor.rgb, vec3(0.0), fogfactor);
}

vec4 ProcessLightMode(Material material)
{
#ifdef SIMPLE2D // uses the fog color to add a color overlay
	#ifdef TM_FOGLAYER
		vec4 frag = material.Base;
		float gray = grayscale(frag);
		vec4 cm = (uObjectColor + gray * (uAddColor - uObjectColor)) * 2;
		frag = vec4(clamp(cm.rgb, 0.0, 1.0), frag.a);
		frag *= vColor;
		frag.rgb = frag.rgb + uFogColor.rgb;
		return frag;
	#else
		vec4 frag = material.Base * vColor;
		frag.rgb = frag.rgb + uFogColor.rgb;
		return frag;
	#endif
#else
	#ifdef TM_FOGLAYER
		#if defined(FOG_BEFORE_LIGHTS) || defined(FOG_AFTER_LIGHTS)
			#if defined(FOG_RADIAL)
				float fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
			#else
				float fogdist = max(16.0, pixelpos.w);
			#endif
			float fogfactor = exp2 (uFogDensity * fogdist);

			return vec4(uFogColor.rgb, (1.0 - fogfactor) * material.Base.a * 0.75 * vColor.a);
		#else
			return vec4(uFogColor.rgb, material.Base.a * 0.75 * vColor.a);
		#endif
	#else
		return getLightColor(material);
	#endif
#endif
}
