
Material ProcessMaterial()
{
	Material material;
	material.Base = getTexel(vTexCoord.st);
	material.Normal = ApplyNormalMap(vTexCoord.st);
	material.Metallic = texture(metallictexture, vTexCoord.st).r;
	material.Roughness = texture(roughnesstexture, vTexCoord.st).r;
	material.AO = texture(aotexture, vTexCoord.st).r;
#if defined(BRIGHTMAP)
	material.Bright = texture(brighttexture, vTexCoord.st);
#endif
	return material;
}
