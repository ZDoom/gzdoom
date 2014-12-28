uniform sampler2D texture2;
uniform vec4 uDetailParms;
uniform vec4 uDetailScale;

vec4 ProcessTexel(vec2 coord)
{
	vec2 st2 = vec2(coord.s * uDetailParms.x + uDetailParms.y, coord.t * uDetailParms.z + uDetailParms.w);
	vec4 detailtex = desaturate(texture(texture2, st2));
	detailtex = clamp(detailtex * uDetailScale.x, uDetailScale.y, uDetailScale.z);
	
	return getTexel(coord.st) * vec4(detailtex.rgb, 1.0);
}

