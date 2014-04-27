in vec4 pixelpos;

void main()
{
#ifndef NO_DISCARD
	// clip plane check is needed for the stencil shader!
	if (pixelpos.y > uClipHeight + 65536.0) discard;
	if (pixelpos.y < uClipHeight - 65536.0) discard;
#endif

	gl_FragColor = vec4(1.0);
}

