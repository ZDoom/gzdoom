
void SetupMaterial(inout Material material)
{
	vec2 texCoord = GetTexCoord();
	SetMaterialProps(material, texCoord);
}
