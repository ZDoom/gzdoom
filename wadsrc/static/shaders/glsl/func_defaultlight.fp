
#if defined(BRIGHTMAP)

vec4 ProcessLight(Material material, vec4 color)
{
	vec4 brightpix = desaturate(material.Bright);
	return vec4(min(color.rgb + brightpix.rgb, 1.0), color.a);
}

#else

vec4 ProcessLight(Material material, vec4 color)
{
	return color;
}

#endif
