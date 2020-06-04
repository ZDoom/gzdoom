
layout(location=0) in vec2 TexCoord;
layout(location=0) out vec4 FragColor;
layout(binding=0) uniform sampler2D SceneTexture;

void main()
{
	vec4 frag = texture(SceneTexture, TexCoord);
	float gray = (frag.r * 0.3 + frag.g * 0.56 + frag.b * 0.14);	
	vec4 cm = uFixedColormapStart + gray * uFixedColormapRange;
	FragColor = vec4(clamp(cm.rgb, 0.0, 1.0), frag.a);
}

