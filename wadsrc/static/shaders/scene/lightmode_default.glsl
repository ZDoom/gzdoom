
vec4 Lightmode_Default()
{
	vec4 color = vColor;

	#if defined(FOG_BEFORE_LIGHTS)

		// calculate fog factor
		#if defined(FOG_RADIAL)
			float fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
		#else
			float fogdist = max(16.0, pixelpos.w);
		#endif
		float fogfactor = exp2 (uFogDensity * fogdist);

		// brightening around the player for light mode 2
		if (fogdist < uLightDist)
		{
			color.rgb *= uLightFactor - (fogdist / uLightDist) * (uLightFactor - 1.0);
		}

		// apply light diminishing through fog equation
		color.rgb = mix(vec3(0.0, 0.0, 0.0), color.rgb, fogfactor);

	#endif

	return color;
}
