uniform sampler2D InputTexture;

in vec2 TexCoord;
out vec4 FragColor;

uniform int nSamples;
uniform float flareDispersal;
uniform float flareHaloWidth;
uniform vec3 flareChromaticDistortion;
uniform int flareMode;
uniform float flareBias;
uniform float flareMul;

vec3 textureDistorted(sampler2D tex, vec2 sample_center, vec2 sample_vector, vec3 distortion) {
	return vec3(
		texture(tex, sample_center + sample_vector * distortion.r).r,
		texture(tex, sample_center + sample_vector * distortion.g).g,
		texture(tex, sample_center + sample_vector * distortion.b).b
	);
}

void main(){
	vec2 texcoord = TexCoord; //-TexCoord + vec2(1.0);
    vec2 imageCenter = vec2(0.5);
    vec2 sampleVector = (imageCenter - texcoord) * flareDispersal;
    vec2 haloVector = normalize(sampleVector) * flareHaloWidth;
    float haloweight = length(vec2(0.5) - fract(texcoord + haloVector)) / length(vec2(0.5));
	haloweight = pow(1.0 - haloweight, 5.0);
    
    vec3 color = texture(InputTexture, TexCoord).rgb;

    if(flareMode == 1) {
        color += max(vec3(0.0), textureDistorted(InputTexture, texcoord + haloVector, haloVector, flareChromaticDistortion).rgb * flareMul + flareBias) * haloweight;
    }
    
    for(int i = 0; i < nSamples; i++){
        vec2 offset = sampleVector * float(i);
        float weight = -(1.0-max(abs(length(vec2(0.5) - offset)), 1.0));
        color += max(vec3(0.0), textureDistorted(InputTexture, texcoord, offset, flareChromaticDistortion).rgb * flareMul + flareBias) * weight;
    }
        
    FragColor = vec4(color, 1.);
}