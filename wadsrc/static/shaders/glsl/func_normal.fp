
vec4 Process(vec4 color)
{
	vec4 pix = getTexel(gl_TexCoord[0].st);
	return pix * color;
}

