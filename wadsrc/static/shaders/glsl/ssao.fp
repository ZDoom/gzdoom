
in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D DepthTexture;

#if defined(MULTISAMPLE)
layout(binding=1) uniform sampler2DMS NormalTexture;
#else
layout(binding=1) uniform sampler2D NormalTexture;
#endif

#if defined(USE_RANDOM_TEXTURE)
layout(binding=2) uniform sampler2D RandomTexture;
#endif

#define PI 3.14159265358979323846

// Calculate eye space position for the specified texture coordinate
vec3 FetchViewPos(vec2 uv)
{
	float z = texture(DepthTexture, uv).x;
    return vec3((UVToViewA * uv + UVToViewB) * z, z);
}

#if defined(MULTISAMPLE)
vec3 SampleNormal(vec2 uv)
{
	ivec2 texSize = textureSize(NormalTexture);
	ivec2 ipos = ivec2(uv * vec2(texSize));
	return texelFetch(NormalTexture, ipos, SampleIndex).xyz * 2.0 - 1.0;
}
#else
vec3 SampleNormal(vec2 uv)
{
	ivec2 texSize = textureSize(NormalTexture, 0);
	ivec2 ipos = ivec2(uv * vec2(texSize));
	return texelFetch(NormalTexture, ipos, 0).xyz * 2.0 - 1.0;
}
#endif

// Look up the eye space normal for the specified texture coordinate
vec3 FetchNormal(vec2 uv)
{
	vec3 normal = SampleNormal(Offset + uv * Scale);
	if (length(normal) > 0.1)
	{
		normal = normalize(normal);
		normal.z = -normal.z;
		return normal;
	}
	else
	{
		return vec3(0.0);
	}
}

// Compute normalized 2D direction
vec2 RotateDirection(vec2 dir, vec2 cossin)
{
    return vec2(dir.x * cossin.x - dir.y * cossin.y, dir.x * cossin.y + dir.y * cossin.x);
}

vec4 GetJitter()
{
#if !defined(USE_RANDOM_TEXTURE)
    return vec4(1,0,1,1);
	//vec3 rand = noise3(TexCoord.x + TexCoord.y);
	//float angle = 2.0 * PI * rand.x / NUM_DIRECTIONS;
	//return vec4(cos(angle), sin(angle), rand.y, rand.z);
#else
    return texture(RandomTexture, gl_FragCoord.xy / RANDOM_TEXTURE_WIDTH);
#endif
}

// Calculates the ambient occlusion of a sample
float ComputeSampleAO(vec3 kernelPos, vec3 normal, vec3 samplePos)
{
	vec3 v = samplePos - kernelPos;
	float distanceSquare = dot(v, v);
	float nDotV = dot(normal, v) * inversesqrt(distanceSquare);
	return clamp(nDotV - NDotVBias, 0.0, 1.0) * clamp(distanceSquare * NegInvR2 + 1.0, 0.0, 1.0);
}

// Calculates the total ambient occlusion for the entire fragment
float ComputeAO(vec3 viewPosition, vec3 viewNormal)
{
    vec4 rand = GetJitter();

	float radiusPixels = RadiusToScreen / viewPosition.z;
	float stepSizePixels = radiusPixels / (NUM_STEPS + 1.0);

	const float directionAngleStep = 2.0 * PI / NUM_DIRECTIONS;
	float ao = 0.0;

    for (float directionIndex = 0.0; directionIndex < NUM_DIRECTIONS; ++directionIndex)
    {
        float angle = directionAngleStep * directionIndex;

        vec2 direction = RotateDirection(vec2(cos(angle), sin(angle)), rand.xy);
        float rayPixels = (rand.z * stepSizePixels + 1.0);

        for (float StepIndex = 0.0; StepIndex < NUM_STEPS; ++StepIndex)
        {
            vec2 sampleUV = round(rayPixels * direction) * InvFullResolution + TexCoord;
            vec3 samplePos = FetchViewPos(sampleUV);
            ao += ComputeSampleAO(viewPosition, viewNormal, samplePos);
            rayPixels += stepSizePixels;
        }
    }

    ao *= AOMultiplier / (NUM_DIRECTIONS * NUM_STEPS);
    return clamp(1.0 - ao * 2.0, 0.0, 1.0);
}

void main()
{
    vec3 viewPosition = FetchViewPos(TexCoord);
	vec3 viewNormal = FetchNormal(TexCoord);
	float occlusion = viewNormal != vec3(0.0) ? ComputeAO(viewPosition, viewNormal) * AOStrength + (1.0 - AOStrength) : 1.0;

	FragColor = vec4(occlusion, viewPosition.z, 0.0, 1.0);
}
