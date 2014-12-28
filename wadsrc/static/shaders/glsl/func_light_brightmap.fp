uniform sampler2D BrightmapTexture;

vec4 ProcessLight(vec4 color)
{
	vec4 brightpix = desaturate(texture(BrightmapTexture, vTexCoord.st));
	return vec4(min (color.rgb + brightpix.rgb, 1.0), color.a);
}
