// This needs to be done better later, but for now this place will have to do.

layout(std140) uniform CustomUniforms
{
	vec2 uSpecularMaterial;
};

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
