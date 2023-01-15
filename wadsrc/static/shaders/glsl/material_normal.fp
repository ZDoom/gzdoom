
vec3 lightContribution(int i, vec3 normal)
{
	vec4 lightpos = lights[i];
	vec4 lightcolor = lights[i+1];
	vec4 lightspot1 = lights[i+2];
	vec4 lightspot2 = lights[i+3];

	float lightdistance = distance(lightpos.xyz, pixelpos.xyz);
	if (lightpos.w < lightdistance)
		return vec3(0.0); // Early out lights touching surface but not this fragment

	vec3 lightdir = normalize(lightpos.xyz - pixelpos.xyz);
	float dotprod = dot(normal, lightdir);
	if (dotprod < -0.0001) return vec3(0.0);	// light hits from the backside. This can happen with full sector light lists and must be rejected for all cases. Note that this can cause precision issues.

	float attenuation = clamp((lightpos.w - lightdistance) / lightpos.w, 0.0, 1.0);

	if (lightspot1.w == 1.0)
		attenuation *= spotLightAttenuation(lightpos, lightspot1.xyz, lightspot2.x, lightspot2.y);

	if (lightcolor.a < 0.0) // Sign bit is the attenuated light flag
	{
		attenuation *= clamp(dotprod, 0.0, 1.0);
	}

	if (attenuation > 0.0) // Skip shadow map test if possible
	{
		attenuation *= shadowAttenuation(lightpos, lightcolor.a);
		return lightcolor.rgb * attenuation;
	}
	else
	{
		return vec3(0.0);
	}
}

vec3 ProcessMaterialLight(Material material, vec3 color)
{
	vec4 dynlight = uDynLightColor;
	vec3 normal = material.Normal;

	if (uLightIndex >= 0)
	{
		ivec4 lightRange = ivec4(lights[uLightIndex]) + ivec4(uLightIndex + 1);
		if (lightRange.z > lightRange.x)
		{
			// modulated lights
			for(int i=lightRange.x; i<lightRange.y; i+=4)
			{
				dynlight.rgb += lightContribution(i, normal);
			}

			// subtractive lights
			for(int i=lightRange.y; i<lightRange.z; i+=4)
			{
				dynlight.rgb -= lightContribution(i, normal);
			}
		}
	}

	vec3 frag;

	if ( uLightBlendMode == 1 )
	{	// COLOR_CORRECT_CLAMPING 
		vec3 lightcolor = color + desaturate(dynlight).rgb;
		frag = material.Base.rgb * ((lightcolor / max(max(max(lightcolor.r, lightcolor.g), lightcolor.b), 1.4) * 1.4));
	}
	else if ( uLightBlendMode == 2 )
	{	// UNCLAMPED 
		frag = material.Base.rgb * (color + desaturate(dynlight).rgb);
	}
	else
	{
		frag = material.Base.rgb * clamp(color + desaturate(dynlight).rgb, 0.0, 1.4);
	}

	if (uLightIndex >= 0)
	{
		ivec4 lightRange = ivec4(lights[uLightIndex]) + ivec4(uLightIndex + 1);
		if (lightRange.w > lightRange.z)
		{
			vec4 addlight = vec4(0.0,0.0,0.0,0.0);

			// additive lights
			for(int i=lightRange.z; i<lightRange.w; i+=4)
			{
				addlight.rgb += lightContribution(i, normal);
			}

			frag = clamp(frag + desaturate(addlight).rgb, 0.0, 1.0);
		}
	}

	return frag;
}
