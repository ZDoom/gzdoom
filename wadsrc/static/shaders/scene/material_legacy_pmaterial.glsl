
Material ProcessMaterial();

void SetupMaterial(inout Material material)
{
	Material legacy = ProcessMaterial();
	material.Base = legacy.Base;
	material.Bright = legacy.Bright;
	material.Normal = legacy.Normal;
	material.Specular = legacy.Specular;
	material.Glossiness = legacy.Glossiness;
	material.SpecularLevel = legacy.SpecularLevel;
	material.Metallic = legacy.Metallic;
	material.Roughness = legacy.Roughness;
	material.AO = legacy.AO;
}
