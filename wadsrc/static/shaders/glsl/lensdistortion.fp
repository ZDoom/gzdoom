/*
	Original Lens Distortion Algorithm from SSontech
	http://www.ssontech.com/content/lensalg.htm

	If (u,v) are the coordinates of a feature in the undistorted perfect
	image plane, then (u', v') are the coordinates of the feature on the
	distorted image plate, ie the scanned or captured image from the
	camera. The distortion occurs radially away from the image center,
	with correction for the image aspect ratio (image_aspect = physical
	image width/height), as follows:

	r2 = image_aspect*image_aspect*u*u + v*v
	f = 1 + r2*(k + kcube*sqrt(r2))
	u' = f*u
	v' = f*v

	The constant k is the distortion coefficient that appears on the lens
	panel and through Sizzle. It is generally a small positive or negative
	number under 1%. The constant kcube is the cubic distortion value found
	on the image preprocessor's lens panel: it can be used to undistort or
	redistort images, but it does not affect or get computed by the solver.
	When no cubic distortion is needed, neither is the square root, saving
	time.

	Chromatic Aberration example,
	using red distord channel with green and blue undistord channel:

	k = vec3(-0.15, 0.0, 0.0);
	kcube = vec3(0.15, 0.0, 0.0);
*/

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D InputTexture;
uniform float Aspect; // image width/height
uniform float Scale;  // 1/max(f)
uniform vec4 k;       // lens distortion coefficient 
uniform vec4 kcube;   // cubic distortion value

void main()
{
	vec2 position = (TexCoord - vec2(0.5));

	vec2 p = vec2(position.x * Aspect, position.y);
	float r2 = dot(p, p);
	vec3 f = vec3(1.0) + r2 * (k.rgb + kcube.rgb * sqrt(r2));

	vec3 x = f * position.x * Scale + 0.5;
	vec3 y = f * position.y * Scale + 0.5;

	vec3 c;
	c.r = texture(InputTexture, vec2(x.r, y.r)).r;
	c.g = texture(InputTexture, vec2(x.g, y.g)).g;
	c.b = texture(InputTexture, vec2(x.b, y.b)).b;

	FragColor = vec4(c, 1.0);
}
