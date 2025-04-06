#pragma once

#include "../Engine/EngineSystem.h"
#include <map>
#include <optional>

namespace CGEngine {
    struct BoneData;

    class Skeleton : public EngineSystem, public IResource {
	public:
        // Default constructor
        Skeleton();

        // Constructor that sets up the bone data.
        Skeleton(map<string, BoneData> boneData);

        /// <summary>
		/// Used to determine if a Skeleton's bone data is equivalent to another set of bone data.
        /// </summary>
        /// <param name="other">The other map of bone data to compare against</param>
        /// <returns>True if they are equivalent</returns>
        bool equals(const map<string, BoneData>& other) const;
        bool hasBone(const string& boneName) const;
        optional<BoneData> getBoneData(const string& boneName) const;
        bool isValid() const;
    private:
        map<string, BoneData> boneData;
    };
}