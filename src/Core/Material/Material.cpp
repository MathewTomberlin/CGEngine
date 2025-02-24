#include "../Engine/Engine.h"
#include "Material.h"

namespace CGEngine {
	Material::Material(MaterialParameters params) : texturePath(params.texturePath), texture(textures->load(params.texturePath)), surfaceColor(params.surfaceColor), shininess(params.shininess), opacity(params.opacity), specularColor(params.specularColor), gammaCorrected(params.gammaCorrected) {
		materialId = world->addMaterial(this);
	};
}