#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace CGEngine {
	class Mesh;
	class Model;
	class Animation;
	struct MeshData;
	struct BoneData;

	struct MeshNodeData {
		MeshNodeData() : materialId(0),transformation(glm::mat4(1.0f)) {};
		MeshNodeData(string nodeName, glm::mat4 transformation) :nodeName(nodeName), transformation(transformation), materialId(0) {};
		MeshData* meshData = nullptr;
		id_t materialId = 0;
		string nodeName;
		vector<MeshNodeData*> children;
		MeshNodeData* parent = nullptr;
		glm::mat4 transformation = glm::mat4(1.0f);
		bool hasMesh() { return meshData != nullptr; }
	};

	struct ImportResult {
		ImportResult() {};
		ImportResult(MeshNodeData* rootNode) : rootNode(rootNode) {};
		MeshNodeData* rootNode = nullptr;
		vector<id_t> materials;
		vector<string> animations;
	};

	// Add new helper struct to track skeletal data during import
	struct SkeletalData {
		map<string, BoneData> allBones;
	};

	class MeshImporter : public EngineSystem {
    public:
		MeshImporter();
		ImportResult importModel(string path, unsigned int options = aiProcess_Triangulate | aiProcess_FlipUVs);
		const aiScene* readFile(string path, unsigned int options);
		// Add direct model creation to support importing animations
		Model* createModel(MeshData* meshData, string name = "");
		Animation* createAnimation(const aiScene* scene, map<string, BoneData> bones, const string& animationName = "");

		static inline glm::mat4 fromAiMatrix4toGlm(const aiMatrix4x4& from) {
			return glm::mat4(
				(double)from.a1, (double)from.b1, (double)from.c1, (double)from.d1,
				(double)from.a2, (double)from.b2, (double)from.c2, (double)from.d2,
				(double)from.a3, (double)from.b3, (double)from.c3, (double)from.d3,
				(double)from.a4, (double)from.b4, (double)from.c4, (double)from.d4
			);
		}

		static inline glm::vec2 aiV2toGlm(const aiVector2D& vec) {
			return glm::vec2(vec.x, vec.y);
		}

		static inline glm::vec2 aiV2toGlm(const aiVector3D& vec) {
			return glm::vec2(vec.x, vec.y);
		}
		
		static inline glm::vec3 aiV3toGlm(const aiVector3D& vec) {
			return glm::vec3(vec.x, vec.y, vec.z);
		}

		static inline glm::quat fromAiQuatToGlm(const aiQuaternion& pOrientation) {
			return glm::quat(pOrientation.w, pOrientation.x, pOrientation.y, pOrientation.z);
		}
        static Color fromAiColor4(aiColor4D* c);

    private:
		vector<id_t> importSceneMaterials(const aiScene* scene);
		ImportResult processNode(aiNode* node, const aiScene* scene, string type, map<string, BoneData>& modelBones, vector<id_t> modelMaterials);
		bool importMesh(MeshNodeData* targetNode, aiNode* node, const aiScene* scene, map<string, BoneData>& modelBones, vector<id_t> modelMaterials);
        vector<string> loadMaterialTextures(aiMaterial* mat, aiTextureType type);
		unsigned int getFormatOptions(string format);
		string getFormat(string path);
		void importAnimations(const aiScene* scene, map<string, BoneData> modelBones, vector<string>& modelAnimations);
        Assimp::Importer modelImporter;
		const aiScene* currentScene = nullptr;
    };
}