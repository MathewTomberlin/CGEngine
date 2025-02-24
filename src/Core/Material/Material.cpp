#include "../Engine/Engine.h"
#include "Material.h"

namespace CGEngine {
	Material::Material(MaterialParameters params) : diffuseTexturePath(params.diffuseTexturePath), diffuseTexture(textures->load(params.diffuseTexturePath)), diffuseTextureUVScale(params.diffuseTextureUVScale), diffuseColor(params.diffuseColor), shininess(params.shininess), opacity(params.opacity), specularColor(params.specularColor), gammaCorrected(params.gammaCorrected) {
		materialId = world->addMaterial(this);
	};
}