uniform sampler2D tex;

in vec4 vTexCoord;
in vec4 vColor;

out vec4 FragColor;

void main()
{
	vec3 color = texture(tex, vTexCoord.st).rgb;

//	/* DEBUG */ if (vTexCoord.x > 0.5)
	{
		// Apply contrast
		float contrast = clamp(vColor.y, 0.1, 3.0);
		color = color.rgb * contrast - (contrast - 1.0) * 0.5;

		// Apply gamma
		float gamma = clamp(vColor.x, 0.1, 4.0);
		color = sign(color) * pow(abs(color), vec3(1.0 / gamma));

		// Apply brightness
		float brightness = clamp(vColor.z, -0.8, 0.8);
		color += brightness * 0.5;

		color = clamp(color, 0.0, 1.0);
	}

	FragColor = vec4(color, 1.0);
}
