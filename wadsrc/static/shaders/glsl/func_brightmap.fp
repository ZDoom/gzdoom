
Material ProcessMaterial()
{
	Material material;
	material.Base = getTexel(vTexCoord.st);
	material.Normal = ApplyNormalMap(vTexCoord.st);
	material.Bright = texture(brighttexture, vTexCoord.st);
	return material;
}
