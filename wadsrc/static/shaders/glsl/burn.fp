
in vec4 vTexCoord;
in vec4 vColor;
layout(location=0) out vec4 FragColor;

void main()
{
	vec4 frag = vColor;

	vec4 t1 = texture(tex, vTexCoord.xy);
	vec4 t2 = texture(texture2, vec2(vTexCoord.x, 1.0-vTexCoord.y));
	
	FragColor = frag * vec4(t1.r, t1.g, t1.b, t2.a);
}
