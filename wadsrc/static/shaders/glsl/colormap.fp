
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D tex;
uniform vec4 uFixedColormapStart;
uniform vec4 uFixedColormapRange;

void main()
{
	vec4 frag = texture(tex, TexCoord);
	float gray = (frag.r * 0.3 + frag.g * 0.56 + frag.b * 0.14);	
	vec4 cm = uFixedColormapStart + gray * uFixedColormapRange;
	FragColor = vec4(clamp(cm.rgb, 0.0, 1.0), frag.a);
}

