uniform sampler2D tex;
uniform sampler2D texture2;

void main()
{
	vec4 frag = gl_Color;

	vec4 t1 = texture2D(texture2, gl_TexCoord[0].xy);
	vec4 t2 = texture2D(tex, vec2(gl_TexCoord[0].x, 1.0-gl_TexCoord[0].y));
	
	gl_FragColor = frag * vec4(t1.r, t1.g, t1.b, t2.a);
}
