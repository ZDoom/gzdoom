
#include "shaders/scene/material.glsl"

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

	frag = getLightColor(material);

	FragColor = frag;
#ifdef GBUFFER_PASS
	FragFog = vec4(AmbientOcclusionColor(), 1.0);
	FragNormal = vec4(vEyeNormal.xyz * 0.5 + 0.5, 1.0);
#endif
}
