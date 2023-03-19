
void SetupMaterial(inout Material material)
{
	SetMaterialProps(material, vTexCoord.st);
	material.Specular = texture(speculartexture, vTexCoord.st).rgb;
	material.Glossiness = uSpecularMaterial.x;
	material.SpecularLevel = uSpecularMaterial.y;
}
