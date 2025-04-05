#version 420

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

struct Material {
    vec4 diffuseColor;
    vec4 specularColor;
    vec2 diffuseTextureUVScale;
    vec2 diffuseTextureScrollSpeed;
    vec2 diffuseTextureOffset;
    vec2 specularTextureUVScale;
    vec2 specularTextureScrollSpeed;
    vec2 specularTextureOffset;
    vec2 opacityTextureUVScale;
    vec2 opacityTextureScrollSpeed;
    vec2 opacityTextureOffset;
    float smoothnessFactor;
    float opacity;
    float alphaCutoff;
    float gamma;
    int useDiffuseTexture;
    int useSpecularTexture;
    int useOpacityTexture;
    int useLighting;
    int useGammaCorrection;
    int opacityMasked;
};
layout(std140, binding = 3) uniform MaterialBlock {
    Material materials[8];
};
struct Light {
    vec4 position;
    vec4 intensities;
    vec4 lightDirection;
    float brightness;
    float attenuation;
    float ambiance;
    float coneAngle;
};
layout(std140, binding = 4) uniform LightBlock {
    Light lights[10];
    int lightCount;
    int lightPadding1;
    int lightPadding2;
    int lightPadding3;
};


layout(std140, binding = 2) uniform TransformBlock {
    mat4 camera;
    mat4 model;
};
layout(std140, binding = 1) uniform BoneBlock {
    mat4 boneMatrices[MAX_BONES];
    int boneCount;
    int padding1;
	int padding2;
	int padding3;
};

in vec3 position;
in vec2 texCoord;
in vec3 vertNormal;
in float materialId;
in ivec4 boneIds;
in vec4 weights;

out vec2 uv;
out vec3 normal;
out vec3 vertex;
out float matId;

void main() {
    vec4 bonePosition = vec4(0.0f);
	vec3 boneNormal = vec3(0.0f);
	float totalWeight = 0.0f;

	//Calculate bone transformation
	for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
        if(boneIds[i] == -1 || boneIds[i] >= MAX_BONES) continue;
		float weight = weights[i];
		totalWeight += weight;

		//Transform position
		vec4 localPosition = boneMatrices[boneIds[i]] * vec4(position,1.0f);
        bonePosition += localPosition * weight;

		//Transform normal
		vec3 localNormal = mat3(boneMatrices[boneIds[i]]) * vertNormal;
		boneNormal += localNormal * weight;
    }

	//Fallback for vertices with no bone influence
	if(totalWeight == 0.0f) {
		bonePosition = vec4(position,1.0f);
		boneNormal = vertNormal;
	}

	//Final transformations
	vec4 worldPos = model * bonePosition;

	uv = texCoord;
	normal = normalize(mat3(model) * boneNormal);
	vertex = worldPos.xyz;
	matId = materialId;

	gl_Position = camera * worldPos;
}