
in vec2 TexCoord;
out vec4 FragColor;

uniform vec2 UVToViewA;
uniform vec2 UVToViewB;
uniform vec2 InvFullResolution;

uniform float NDotVBias;
uniform float NegInvR2;
uniform float RadiusToScreen;
uniform float AOMultiplier;

uniform sampler2D DepthTexture;

#if USE_RANDOM_TEXTURE
uniform sampler2D RandomTexture;
#endif

#define NUM_DIRECTIONS 8.0
#define NUM_STEPS 4.0

#define PI 3.14159265358979323846

// Calculate eye space position for the specified texture coordinate
vec3 FetchViewPos(vec2 uv)
{
    float z = texture(DepthTexture, uv).x;
    return vec3((UVToViewA * uv + UVToViewB) * z, z);
}

vec3 MinDiff(vec3 p, vec3 pr, vec3 pl)
{
    vec3 v1 = pr - p;
    vec3 v2 = p - pl;
    return (dot(v1, v1) < dot(v2, v2)) ? v1 : v2;
}

// Reconstruct eye space normal from nearest neighbors
vec3 ReconstructNormal(vec3 p)
{
    vec3 pr = FetchViewPos(TexCoord + vec2(InvFullResolution.x, 0));
    vec3 pl = FetchViewPos(TexCoord + vec2(-InvFullResolution.x, 0));
    vec3 pt = FetchViewPos(TexCoord + vec2(0, InvFullResolution.y));
    vec3 pb = FetchViewPos(TexCoord + vec2(0, -InvFullResolution.y));
    return normalize(cross(MinDiff(p, pr, pl), MinDiff(p, pt, pb)));
}

// Compute normalized 2D direction
vec2 RotateDirection(vec2 dir, vec2 cossin)
{
    return vec2(dir.x * cossin.x - dir.y * cossin.y, dir.x * cossin.y + dir.y * cossin.x);
}

vec4 GetJitter()
{
#if !USE_RANDOM_TEXTURE
    return vec4(1,0,1,1);
	//vec3 rand = noise3(TexCoord.x + TexCoord.y);
	//float angle = 2.0 * PI * rand.x / NUM_DIRECTIONS;
	//return vec4(cos(angle), sin(angle), rand.y, rand.z);
#else
	#define RANDOM_TEXTURE_WIDTH 4.0
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
    vec3 viewNormal = ReconstructNormal(viewPosition);
	float occlusion = ComputeAO(viewPosition, viewNormal);
	//FragColor = vec4(viewPosition.x * 0.001 + 0.5, viewPosition.y * 0.001 + 0.5, viewPosition.z * 0.001, 1.0);
	//FragColor = vec4(viewNormal.x * 0.5 + 0.5, viewNormal.y * 0.5 + 0.5, viewNormal.z * 0.5 + 0.5, 1.0);
	//FragColor = vec4(occlusion, viewPosition.z, 0.0, 1.0);
	FragColor = vec4(occlusion, occlusion, occlusion, 1.0);
}
