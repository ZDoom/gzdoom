
vec4 Process(vec4);

void SetupMaterial(inout Material material)
{
	material.Base = Process(vec4(1.0));
	material.Normal = ApplyNormalMap(vTexCoord.st);
	material.Bright = texture(brighttexture, vTexCoord.st);
}
