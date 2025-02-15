#pragma once

#include "../../Core/Behavior/Behavior.h"
#include "../../Core/World/World.h"
#include <functional>
using namespace std;

namespace CGEngine {
	class BoundsBehavior : public Behavior {
	public:
		BoundsBehavior(Body* owner, function<FloatRect(const Body*)> getGlobalBounds, function<FloatRect(const Body*)> getLocalBounds);
		function<FloatRect(const Body*)> getLocalBounds = [](const Body* body) {
			return FloatRect({ 0,0 }, { 1,1 });
		};
		function<FloatRect(const Body*)> getGlobalBounds = [](const Body* body) {
			return FloatRect({ 0,0 }, { 1,1 });
		};

	};
}