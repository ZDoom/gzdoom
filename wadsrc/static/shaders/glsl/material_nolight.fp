
vec3 ProcessMaterial(vec3 material, vec3 color)
{
	return material * clamp(color + desaturate(uDynLightColor).rgb, 0.0, 1.4);
}
