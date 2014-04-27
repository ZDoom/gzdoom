uniform sampler2D texture2;

vec4 Process(vec4 color)
{
	vec4 pix = getTexel(gl_TexCoord[0].st);
	return pix * color;
}

vec4 ProcessLight(vec4 color)
{
	vec4 brightpix = desaturate(texture2D(texture2, gl_TexCoord[0].st));
	return vec4(min (color.rgb + brightpix.rgb, 1.0), color.a);
}
