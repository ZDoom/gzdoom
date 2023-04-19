
float VanillaWallColormap(float lightnum, float z, vec3 normal)
{
	// R_ScaleFromGlobalAngle calculation
	float projection = 160.0; // projection depends on SCREENBLOCKS!! 160 is the fullscreen value
	vec2 line_v1 = pixelpos.xz; // in vanilla this is the first curline vertex
	vec2 line_normal = normal.xz;
	float texscale = projection * clamp(dot(normalize(uCameraPos.xz - line_v1), line_normal), 0.0, 1.0) / z;

	float lightz = clamp(16.0 * texscale, 0.0, 47.0);

	// scalelight[lightnum][lightz] lookup
	float startmap = (15.0 - lightnum) * 4.0;
	return startmap - lightz * 0.5;
}

float VanillaPlaneColormap(float lightnum, float z)
{
	float lightz = clamp(z / 16.0f, 0.0, 127.0);

	// zlight[lightnum][lightz] lookup
	float startmap = (15.0 - lightnum) * 4.0;
	float scale = 160.0 / (lightz + 1.0);
	return startmap - scale * 0.5;
}

float VanillaColormap(float light, float z)
{
	float lightnum = clamp(light * 15.0, 0.0, 15.0);

	if (dot(vWorldNormal.xyz, vWorldNormal.xyz) > 0.5)
	{
		vec3 normal = normalize(vWorldNormal.xyz);
		return mix(VanillaWallColormap(lightnum, z, normal), VanillaPlaneColormap(lightnum, z), abs(normal.y));
	}
	else // vWorldNormal is not set on sprites
	{
		return VanillaPlaneColormap(lightnum, z);
	}
}

vec4 Lightmode_Vanilla()
{
	// z is the depth in view space, positive going into the screen
	#if defined(SWLIGHT_RADIAL)
		float z = distance(pixelpos.xyz, uCameraPos.xyz);
	#else
		float z = pixelpos.w;
	#endif

	float colormap = VanillaColormap(uLightLevel, z);

	#if defined(SWLIGHT_BANDED)
		colormap = floor(colormap) + 0.5;
	#endif

	// Result is the normalized colormap index (0 bright .. 1 dark)
	float newlightlevel = 1.0 - clamp(colormap, 0.0, 31.0) / 32.0;

	vec4 color = vColor;
	color.rgb *= newlightlevel;
	return color;
}
