
void SetupMaterial(inout Material material)
{
	material.Base = ProcessTexel();
	material.Normal = ApplyNormalMap(vTexCoord.st);
	material.Bright = texture(brighttexture, vTexCoord.st);
}
