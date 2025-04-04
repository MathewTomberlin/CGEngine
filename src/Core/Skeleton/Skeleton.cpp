#include "../World/Renderer.h"
#include "Skeleton.h"

namespace CGEngine {
	Skeleton::Skeleton() {};

	// Constructor that sets up the bone data.
	Skeleton::Skeleton(map<string, BoneData> boneData) : boneData(boneData) {};

	/// <summary>
	/// Used to determine if a Skeleton's bone data is equivalent to another set of bone data.
	/// </summary>
	/// <param name="other">The other map of bone data to compare against</param>
	/// <returns>True if they are equivalent</returns>
	bool Skeleton::equals(const map<string, BoneData>& other) const {
		return boneData == other;
	}

	bool Skeleton::hasBone(const string& boneName) const {
		return boneData.find(boneName) != boneData.end();
	}

	optional<BoneData> Skeleton::getBoneData(const string& boneName) const {
		auto it = boneData.find(boneName);
		if (it != boneData.end()) {
			return it->second;
		}
		return nullopt;
	}

	bool Skeleton::isValid() const {
		return boneData.size() > 0;
	}
}