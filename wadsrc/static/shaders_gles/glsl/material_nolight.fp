
vec3 ProcessMaterialLight(Material material, vec3 color)
{
	return material.Base.rgb * clamp(color + desaturate(uDynLightColor).rgb, 0.0, 1.4);
}
