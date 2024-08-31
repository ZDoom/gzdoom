
varying vec2 TexCoord;

uniform sampler2D InputTexture;
uniform sampler2D DitherTexture;

vec4 ApplyGamma(vec4 c)
{
	vec3 valgray;

	valgray = mix(vec3(dot(c.rgb, vec3(0.3,0.56,0.14))), c.rgb, Saturation);

	vec3 val = valgray * Contrast - (Contrast - 1.0) * 0.5;
	val += Brightness * 0.5;
	val = pow(max(val, vec3(0.0)), vec3(InvGamma));
	return vec4(val, c.a);
}

//vec4 Dither(vec4 c)
//{
//	if (ColorScale == 0.0)
//		return c;
//	vec2 texSize = vec2(textureSize(DitherTexture, 0));
//	float threshold = texture2D(DitherTexture, gl_FragCoord.xy / texSize).r;
//	return vec4(floor(c.rgb * ColorScale + threshold) / ColorScale, c.a);
//}


void main()
{
	gl_FragColor =  ApplyGamma(texture2D(InputTexture, UVOffset + TexCoord * UVScale));
}
