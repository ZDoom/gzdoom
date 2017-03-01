
precision mediump float;

in vec4 PixelColor0;
in vec4 PixelColor1;
in vec4 PixelTexCoord0;

out vec4 FragColor;

uniform sampler2D Image;
uniform sampler2D Palette;
uniform sampler2D NewScreen;
uniform sampler2D Burn;

uniform vec4 Desaturation; // { Desat, 1 - Desat }
uniform vec4 PaletteMod;
uniform vec4 Weights; // RGB->Gray weighting { 77/256.0, 143/256.0, 37/256.0, 1 }
uniform vec4 Gamma;

vec4 TextureLookup(vec2 tex_coord)
{
#if defined(PALTEX)
	float index = texture(Image, tex_coord).x;
	index = index * PaletteMod.x + PaletteMod.y;
	return texture(Palette, vec2(index, 0.5));
#else
	return texture(Image, tex_coord);
#endif
}

vec4 Invert(vec4 rgb)
{
#if defined(INVERT)
	rgb.rgb = Weights.www - rgb.xyz;
#endif
	return rgb;
}

float Grayscale(vec4 rgb)
{
	return dot(rgb.rgb, Weights.rgb);
}

vec4 SampleTexture(vec2 tex_coord)
{
	return Invert(TextureLookup(tex_coord));
}

// Normal color calculation for most drawing modes.

vec4 NormalColor(vec2 tex_coord, vec4 Flash, vec4 InvFlash)
{
	return Flash + SampleTexture(tex_coord) * InvFlash;
}

// Copy the red channel to the alpha channel. Pays no attention to palettes.

vec4 RedToAlpha(vec2 tex_coord, vec4 Flash, vec4 InvFlash)
{
	vec4 color = Invert(texture(Image, tex_coord));
	color.a = color.r;
	return Flash + color * InvFlash;
}

// Just return the value of c0.

vec4 VertexColor(vec4 color)
{
	return color;
}

// Emulate one of the special colormaps. (Invulnerability, gold, etc.)

vec4 SpecialColormap(vec2 tex_coord, vec4 start, vec4 end)
{
	vec4 color = SampleTexture(tex_coord);
	vec4 range = end - start;
	// We can't store values greater than 1.0 in a color register, so we multiply
	// the final result by 2 and expect the caller to divide the start and end by 2.
	color.rgb = 2.0 * (start + Grayscale(color) * range).rgb;
	// Duplicate alpha semantics of NormalColor.
	color.a = start.a + color.a * end.a;
	return color;
}

// In-game colormap effect: fade to a particular color and multiply by another, with 
// optional desaturation of the original color. Desaturation is stored in c1.
// Fade level is packed int fade.a. Fade.rgb has been premultiplied by alpha.
// Overall alpha is in color.a.
vec4 InGameColormap(vec2 tex_coord, vec4 color, vec4 fade)
{
	vec4 rgb = SampleTexture(tex_coord);

	// Desaturate
#if defined(DESAT)
	vec3 intensity;
	intensity.rgb = vec3(Grayscale(rgb) * Desaturation.x);
	rgb.rgb = intensity.rgb + rgb.rgb * Desaturation.y;
#endif

	// Fade
	rgb.rgb = rgb.rgb * fade.aaa + fade.rgb;

	// Shade and Alpha
	rgb = rgb * color;

	return rgb;
}

// Windowed gamma correction.

vec4 GammaCorrection(vec2 tex_coord)
{
	vec4 color = texture(Image, tex_coord);
	color.rgb = pow(color.rgb, Gamma.rgb);
	return color;
}

// The burn wipe effect.

vec4 BurnWipe(vec4 coord)
{
	vec4 color = texture(NewScreen, coord.xy);
	vec4 alpha = texture(Burn, coord.zw);
	color.a = alpha.r * 2.0;
	return color;
}

void main()
{
#if defined(ENORMALCOLOR)
	FragColor = NormalColor(PixelTexCoord0.xy, PixelColor0, PixelColor1);
#elif defined(EREDTOALPHA)
	FragColor = RedToAlpha(PixelTexCoord0.xy, PixelColor0, PixelColor1);
#elif defined(EVERTEXCOLOR)
	FragColor = VertexColor(PixelColor0);
#elif defined(ESPECIALCOLORMAP)
	FragColor = SpecialColormap(PixelTexCoord0.xy, PixelColor0, PixelColor1);
#elif defined(EINGAMECOLORMAP)
	FragColor = InGameColormap(PixelTexCoord0.xy, PixelColor0, PixelColor1);
#elif defined(EBURNWIPE)
	FragColor = BurnWipe(PixelTexCoord0);
#elif defined(EGAMMACORRECTION)
	FragColor = GammaCorrection(PixelTexCoord0.xy);
#else
	#error Entry point define is missing
#endif
}
