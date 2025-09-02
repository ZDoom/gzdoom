vec4 Process(vec4 color)
{
	float u_gamma = 1.0;
	float u_black_point = 0.0;
	float u_white_point = 1.0;

	float checker_size = 32.0;
	vec2 checker_coord = floor(gl_FragCoord.xy / checker_size);
	vec3 base_color;

	float col = floor(mod(checker_coord.x, 4.0));
	float row = floor(mod(checker_coord.y, 4.0));
	float dither_pattern = mod(gl_FragCoord.x + gl_FragCoord.y, 2.0);

	vec3 colorA = vec3(0.5);
	vec3 colorB = mix(vec3(0.0), vec3(1.0), dither_pattern);
	vec3 colorC = mix(vec3(u_black_point), vec3(1.0), dither_pattern);
	vec3 colorD = mix(vec3(0.0), vec3(u_white_point), dither_pattern);

	if (row < 1.0) { // Row 0: A B C D
		if (col < 1.0) { base_color = colorA; }
		else if (col < 2.0) { base_color = colorB; }
		else if (col < 3.0) { base_color = colorC; }
		else { base_color = colorD; }
	} else if (row < 2.0) { // Row 1: A B D C
		if (col < 1.0) { base_color = colorA; }
		else if (col < 2.0) { base_color = colorB; }
		else if (col < 3.0) { base_color = colorD; }
		else { base_color = colorC; }
	} else if (row < 3.0) { // Row 2: C D A B
		if (col < 1.0) { base_color = colorC; }
		else if (col < 2.0) { base_color = colorD; }
		else if (col < 3.0) { base_color = colorA; }
		else { base_color = colorB; }
	} else { // Row 3: D C A B
		if (col < 1.0) { base_color = colorD; }
		else if (col < 2.0) { base_color = colorC; }
		else if (col < 3.0) { base_color = colorA; }
		else { base_color = colorB; }
	}

	vec3 final_color = pow(base_color, vec3(1.0/u_gamma)); //InvGamma is not defined in material hardware shaders
	return vec4(clamp(final_color, 0.0, 1.0), 1.0);
}
