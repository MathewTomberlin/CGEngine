#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace CGEngine {
	class Mesh;
	class Model;
	struct MeshData;

	struct MeshNodeData {
		MeshData* meshData = nullptr;
		id_t materialId;
		string nodeName;
		vector<MeshNodeData*> children;
		MeshNodeData* parent = nullptr;
		glm::mat4 transformation;
	};

	struct ImportResult {
		MeshNodeData* rootNode = nullptr;
		bool skeletalMesh = false;
	};

	class MeshImporter {
    public:
		ImportResult importModel(string path, unsigned int options = aiProcess_Triangulate | aiProcess_FlipUVs);
		const aiScene* readFile(string path, unsigned int options);
		// Add direct model creation to support importing animations
		Model* createModel(MeshData* meshData, string name = "");
		// Add animation import to manually created model
		bool importAnimation(const string& path, Model* targetModel);

		static inline glm::mat4 fromAiMatrix4toGlm(const aiMatrix4x4& from) {
			return glm::mat4(
				(double)from.a1, (double)from.b1, (double)from.c1, (double)from.d1,
				(double)from.a2, (double)from.b2, (double)from.c2, (double)from.d2,
				(double)from.a3, (double)from.b3, (double)from.c3, (double)from.d3,
				(double)from.a4, (double)from.b4, (double)from.c4, (double)from.d4
			);
		}

		static inline glm::vec3 fromAiVec3toGlm(const aiVector3D& vec) {
			return glm::vec3(vec.x, vec.y, vec.z);
		}

		static inline glm::quat fromAiQuatToGlm(const aiQuaternion& pOrientation) {
			return glm::quat(pOrientation.w, pOrientation.x, pOrientation.y, pOrientation.z);
		}
        static Color fromAiColor4(aiColor4D* c);

    private:
		ImportResult processNode(aiNode* node, const aiScene* scene, string type);
        vector<string> loadMaterialTextures(aiMaterial* mat, aiTextureType type);

        Assimp::Importer modelImporter;
    };
}