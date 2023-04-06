
void main()
{
#ifdef NO_CLIPDISTANCE_SUPPORT
	if (ClipDistanceA.x < 0 || ClipDistanceA.y < 0 || ClipDistanceA.z < 0 || ClipDistanceA.w < 0 || ClipDistanceB.x < 0) discard;
#endif

	Material material = CreateMaterial();
	vec4 frag = material.Base;

#ifndef NO_ALPHATEST
	if (frag.a <= uAlphaThreshold) discard;
#endif

#ifdef SIMPLE2D // uses the fog color to add a color overlay
	#ifdef TM_FOGLAYER
		float gray = grayscale(frag);
		vec4 cm = (uObjectColor + gray * (uAddColor - uObjectColor)) * 2;
		frag = vec4(clamp(cm.rgb, 0.0, 1.0), frag.a);
		frag *= vColor;
		frag.rgb = frag.rgb + uFogColor.rgb;
	#else
		frag *= vColor;
		frag.rgb = frag.rgb + uFogColor.rgb;
	#endif
#else
	#ifdef TM_FOGLAYER
		float fogdist = 0.0;
		float fogfactor = 0.0;

		// calculate fog factor
		if (uFogEnabled != 0)
		{
			if (uFogEnabled == 1 || uFogEnabled == -1) 
			{
				fogdist = max(16.0, pixelpos.w);
			}
			else 
			{
				fogdist = max(16.0, distance(pixelpos.xyz, uCameraPos.xyz));
			}
			fogfactor = exp2 (uFogDensity * fogdist);
		}

		frag = vec4(uFogColor.rgb, (1.0 - fogfactor) * frag.a * 0.75 * vColor.a);
	#else
		frag = getLightColor(material);
	#endif
#endif

	FragColor = frag;
#ifdef GBUFFER_PASS
	FragFog = vec4(AmbientOcclusionColor(), 1.0);
	FragNormal = vec4(vEyeNormal.xyz * 0.5 + 0.5, 1.0);
#endif
}
