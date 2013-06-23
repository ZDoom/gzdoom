uniform sampler2D texture2;

vec4 Process(vec4 color)
{
	vec4 brightpix = desaturate(texture2D(texture2, gl_TexCoord[0].st));
	vec4 texel = getTexel(gl_TexCoord[0].st);
	return vec4(texel.rgb * min (color.rgb + brightpix.rgb, 1.0), texel.a*color.a);
}

