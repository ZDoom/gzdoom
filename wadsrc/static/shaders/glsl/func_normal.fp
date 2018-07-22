
Material ProcessMaterial()
{
	Material material;
	material.Base = getTexel(vTexCoord.st);
	material.Normal = ApplyNormalMap(vTexCoord.st);
	return material;
}
