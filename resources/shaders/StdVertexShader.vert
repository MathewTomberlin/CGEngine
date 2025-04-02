#version 330

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

uniform mat4 camera;
uniform mat4 model;
uniform mat4 boneMatrices[MAX_BONES];

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