in vec4 pixelpos;

void main()
{
#ifndef NO_DISCARD
	// clip plane emulation for plane reflections. These are always perfectly horizontal so a simple check of the pixelpos's y coordinate is sufficient.
	if (pixelpos.y > uClipHeight + 65536.0) discard;
	if (pixelpos.y < uClipHeight - 65536.0) discard;
#endif

	gl_FragColor = vec4(1.0);
}

