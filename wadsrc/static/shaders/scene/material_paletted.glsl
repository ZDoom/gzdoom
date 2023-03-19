
void SetupMaterial(inout Material material)
{
	float index = getTexel(vTexCoord.st).r;
 	index = ((index * 255.0) + 0.5) / 256.0;
	vec4 tex = texture(texture2, vec2(index, 0.5));
	tex.a = 1.0;
	material.Base = tex;
}
