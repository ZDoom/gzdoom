uniform sampler2D InputTexture;

in vec2 TexCoord;
out vec4 FragColor;

uniform int nSamples;
uniform float flareDispersal;
uniform float flareHaloWidth;
uniform vec3 flareChromaticDistortion;

vec3 textureDistorted(sampler2D tex, vec2 sample_center, vec2 sample_vector, vec3 distortion) {
	return vec3(
		texture(tex, sample_center + sample_vector * distortion.r).r,
		texture(tex, sample_center + sample_vector * distortion.g).g,
		texture(tex, sample_center + sample_vector * distortion.b).b
	);
}

void main(){
    vec2 imageCenter = vec2(0.5);
    vec2 sampleVector = (imageCenter - TexCoord) * flareDispersal;
    vec2 haloVector = normalize(sampleVector) * flareHaloWidth;
    
    vec3 color = texture(InputTexture, TexCoord).rgb;//textureDistorted(InputTexture, TexCoord + haloVector, haloVector, flareChromaticDistortion).rgb * 3.0;
    for(int i = 0; i < nSamples; i++){
        vec2 offset = sampleVector * float(i);
        color += max(vec3(0.0), textureDistorted(InputTexture, abs(TexCoord + offset - 0.5), offset, flareChromaticDistortion).rgb * 0.5 - 0.2);
    }
        
    FragColor = vec4(color, 1.);
}