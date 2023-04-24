
void main()
{
	//
	// calculate fog factor
	//

	#if defined(FOG_RADIAL)
		float fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
	#else
		float fogdist = max(16.0, pixelpos.w);
	#endif
	float fogfactor = exp2 (uFogDensity * fogdist);

	FragColor = vec4(uFogColor.rgb, 1.0 - fogfactor);
#ifdef GBUFFER_PASS
	FragFog = vec4(0.0, 0.0, 0.0, 1.0);
	FragNormal = vec4(0.5, 0.5, 0.5, 1.0);
#endif
}

