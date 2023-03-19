
struct BonesResult
{
	vec3 Normal;
	vec4 Position;
};

BonesResult ApplyBones();

#if !defined(SIMPLE)
vec3 GetAttrNormal()
{
	#ifdef HAS_UNIFORM_VERTEX_DATA
		if ((useVertexData & 2) == 0)
			return uVertexNormal.xyz;
		else
			return mix(aNormal.xyz, aNormal2.xyz, uInterpolationFactor);
	#else
		return mix(aNormal.xyz, aNormal2.xyz, uInterpolationFactor);
	#endif
}

void AddWeightedBone(uint boneIndex, float weight, inout vec4 position, inout vec3 normal)
{
	if (weight != 0.0)
	{
		mat4 transform = bones[uBoneIndexBase + int(boneIndex)];
		mat3 rotation = mat3(transform);
		position += (transform * aPosition) * weight;
		normal += (rotation * aNormal.xyz) * weight;
	}
}

BonesResult ApplyBones()
{
	BonesResult result;
	if (uBoneIndexBase >= 0 && aBoneWeight != vec4(0.0))
	{
		result.Position = vec4(0.0);
		result.Normal = vec3(0.0);

		// We use low precision input for our bone weights. Rescale so the sum still is 1.0
		float totalWeight = aBoneWeight.x + aBoneWeight.y + aBoneWeight.z + aBoneWeight.w;
		float weightMultiplier = 1.0 / totalWeight;
		vec4 boneWeight = aBoneWeight * weightMultiplier;

		AddWeightedBone(aBoneSelector.x, boneWeight.x, result.Position, result.Normal);
		AddWeightedBone(aBoneSelector.y, boneWeight.y, result.Position, result.Normal);
		AddWeightedBone(aBoneSelector.z, boneWeight.z, result.Position, result.Normal);
		AddWeightedBone(aBoneSelector.w, boneWeight.w, result.Position, result.Normal);

		result.Position.w = 1.0; // For numerical stability
	}
	else
	{
		result.Position = aPosition;
		result.Normal = GetAttrNormal();
	}
	return result;
}

#else

BonesResult ApplyBones()
{
	BonesResult result;
	result.Position = aPosition;
	return result;
}

#endif
