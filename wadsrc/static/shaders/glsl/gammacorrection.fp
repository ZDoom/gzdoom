uniform sampler2D tex;
uniform sampler2D texture2;

in vec4 vTexCoord;

void applyGamma(int channel)
{
	vec4 color = texture(texture2, vec2(FragColor[channel], 0.0));
	FragColor[channel] = color[channel];
}

void main()
{
	FragColor = texture(tex, vTexCoord.st);

//	/* DEBUG */ if (vTexCoord.x > 0.5)
	{
		applyGamma(0);
		applyGamma(1);
		applyGamma(2);
	}
	
	FragColor.a = 1.0;
}
