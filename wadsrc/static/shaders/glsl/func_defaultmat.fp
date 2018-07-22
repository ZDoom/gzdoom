
Material ProcessMaterial()
{
	Material material;
	material.Base = ProcessTexel();
	material.Normal = ApplyNormalMap(vTexCoord.st);
#if defined(BRIGHTMAP)
	material.Bright = texture(brighttexture, vTexCoord.st);
#endif
	return material;
}
