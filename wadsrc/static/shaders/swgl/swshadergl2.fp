uniform sampler2D tex;
uniform sampler2D palette;
uniform vec4 uColor1;
uniform vec4 uColor2;

vec4 TextureLookup(vec2 tex_coord)
{
#ifdef PAL_TEX
		float index = texture2D(tex, tex_coord).x;
		index = ((index * 255.0) + 0.5) / 256.0;
		return texture2D(palette, vec2(index, 0.5));
#else
		return texture2D(tex, tex_coord);
#endif
}

float grayscale(vec4 rgb)
{
	return dot(rgb.rgb, vec3(0.4, 0.56, 0.14));
}

void main()
{
#ifndef SPECIALCM
		gl_FragColor = TextureLookup(gl_TexCoord[0].xy);
#else
		vec4 frag = TextureLookup(gl_TexCoord[0].xy);
		float gray = grayscale(frag);
		vec4 cm = uColor1 + gray * uColor2;
		gl_FragColor = vec4(clamp(cm.rgb, 0.0, 1.0), 1.0);
#endif
}
