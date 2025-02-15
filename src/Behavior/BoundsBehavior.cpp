#include "BoundsBehavior.h"

namespace CGEngine {
	BoundsBehavior::BoundsBehavior(Body* owning, function<FloatRect(const Body*)> getGlobalBounds, function<FloatRect(const Body*)> getLocalBounds) : Behavior(owning) {
		this->getGlobalBounds = getGlobalBounds;
		this->getLocalBounds = getLocalBounds;
		getOwner()->getBodyGlobalBounds = getGlobalBounds;
		getOwner()->getBodyLocalBounds = getLocalBounds;
	}
}