#if defined(SPECULAR)
uniform sampler2D texture4;
#define brighttexture texture4
#elif defined(PBR)
uniform sampler2D texture6;
#define brighttexture texture6
#else
uniform sampler2D texture2;
#define brighttexture texture2
#endif

vec4 ProcessTexel()
{
	return getTexel(vTexCoord.st);
}

vec4 ProcessLight(vec4 color)
{
	vec4 brightpix = desaturate(texture(brighttexture, vTexCoord.st));
	return vec4(min (color.rgb + brightpix.rgb, 1.0), color.a);
}
