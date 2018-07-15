
Material ProcessMaterial()
{
	Material material;
	material.Base = getTexel(vTexCoord.st);
	material.Normal = ApplyNormalMap(vTexCoord.st);
	material.Specular = texture(speculartexture, vTexCoord.st).rgb;
	material.Glossiness = uSpecularMaterial.x;
	material.SpecularLevel = uSpecularMaterial.y;
#if defined(BRIGHTMAP)
	material.Bright = texture(brighttexture, vTexCoord.st);
#endif
	return material;
}
