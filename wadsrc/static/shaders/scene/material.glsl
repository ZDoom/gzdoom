
#include "shaders/scene/material_getTexel.glsl"
#include "shaders/scene/material_normalmap.glsl"

struct Material
{
	vec4 Base;
	vec4 Bright;
	vec4 Glow;
	vec3 Normal;
	vec3 Specular;
	float Glossiness;
	float SpecularLevel;
	float Metallic;
	float Roughness;
	float AO;
};

vec4 Process(vec4 color);
void SetupMaterial(inout Material mat);
vec3 ProcessMaterialLight(Material material, vec3 color);
vec2 GetTexCoord();

Material CreateMaterial()
{
	Material material;
	material.Base = vec4(0.0);
	material.Bright = vec4(0.0);
	material.Glow = vec4(0.0);
	material.Normal = vec3(0.0);
	material.Specular = vec3(0.0);
	material.Glossiness = 0.0;
	material.SpecularLevel = 0.0;
	material.Metallic = 0.0;
	material.Roughness = 0.0;
	material.AO = 0.0;
	SetupMaterial(material);
	return material;
}

void SetMaterialProps(inout Material material, vec2 texCoord)
{
#ifdef NPOT_EMULATION
	if (uNpotEmulation.y != 0.0)
	{
		float period = floor(texCoord.t / uNpotEmulation.y);
		texCoord.s += uNpotEmulation.x * floor(mod(texCoord.t, uNpotEmulation.y));
		texCoord.t = period + mod(texCoord.t, uNpotEmulation.y);
	}
#endif	
	material.Base = getTexel(texCoord.st); 
	material.Normal = ApplyNormalMap(texCoord.st);

// OpenGL doesn't care, but Vulkan pukes all over the place if these texture samplings are included in no-texture shaders, even though never called.
#ifndef NO_LAYERS
	#if defined(TEXF_Brightmap)
		material.Bright = desaturate(texture(brighttexture, texCoord.st));
	#endif

	#if defined(TEXF_Detailmap)
		vec4 Detail = texture(detailtexture, texCoord.st * uDetailParms.xy) * uDetailParms.z;
		material.Base.rgb *= Detail.rgb;
	#endif

	#if defined(TEXF_Glowmap)
		material.Glow = desaturate(texture(glowtexture, texCoord.st));
	#endif
#endif
}
