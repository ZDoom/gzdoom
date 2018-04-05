in vec4 vTexCoord;
out vec4 FragColor;

vec4 TextureLookup(vec2 tex_coord)
{
	if (uTextureMode == 1)
	{
		float index = texture(tex, tex_coord).x;
		index = index * 256.0 + 0.5;	// We only have 256 color palettes here.
		return texture(texture2, vec2(index, 0.5));
	}
	else
	{
		return texture(tex, tex_coord);
	}
}

void main()
{
	if (uFixedColormap == 0)
	{
		FragColor = TextureLookup(vTexCoord.xy);
	}
	else
	{
		vec4 frag = TextureLookup(vTexCoord.xy);
		float gray = dot(frag.rgb, vec3(0.4, 0.56, 0.14));
		vec4 cm = uFixedColormapStart + gray * uFixedColormapRange;
		FragColor = vec4(clamp(cm.rgb, 0.0, 1.0), 1.0);
	}
}
