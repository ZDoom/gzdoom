uniform sampler2D backbuffer;
uniform sampler2D gammaTable;

void main()
{
	vec3 color = texture2D(backbuffer, gl_TexCoord[0].xy).rgb;

//	/* DEBUG */ vec2 uv = gl_TexCoord[0].xy; if (uv.x<0.50) {

	gl_FragColor.r = texture2D(gammaTable, vec2(color.r, 0.0)).r;
	gl_FragColor.g = texture2D(gammaTable, vec2(color.g, 0.0)).g;
	gl_FragColor.b = texture2D(gammaTable, vec2(color.b, 0.0)).b;

//	/* DEBUG */ } else gl_FragColor.rgb = color;

	gl_FragColor.a = 1.0;
}
