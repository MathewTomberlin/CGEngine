#pragma once
#include "Types.h"
#include "UniqueIntegerStack.h"

namespace CGEngine {
	template <typename DomainKey, typename DomainValue>
	class UniqueDomain {
	public:
		UniqueDomain(size_t maxSize = 0) {
			ids = UniqueIntegerStack<DomainKey>(maxSize);
		}

		DomainKey add(DomainValue value) {
			DomainKey key = ids.take();
			domain[key] = value;
			return key;
		}

		void remove(DomainKey key) {
			if (domain.find(key) != domain.end()) {
				ids.give(key);
				domain.erase(key);
			}
		}

		DomainValue get(DomainKey key) {
			if (domain.find(key) != domain.end()) {
				return domain[key];
			}
		}

		void forEach(function<void(DomainValue)> function) {
			for (auto iterator = domain.begin(); iterator != domain.end(); ++iterator) {
				function((*iterator).second);
			}
		}
	private:
		map<DomainKey, DomainValue> domain;
		UniqueIntegerStack<DomainKey> ids = UniqueIntegerStack<DomainKey>(0U);
	};
}