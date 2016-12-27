uniform sampler2D texture2;

vec4 ProcessTexel()
{
	return getTexel(vTexCoord.st);
}

vec4 ProcessLight(vec4 color)
{
	vec4 brightpix = desaturate(texture(texture2, vTexCoord.st));
	return vec4(min (color.rgb + brightpix.rgb, 1.0), color.a);
}
