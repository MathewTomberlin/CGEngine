#pragma once
#include "Types.h"
#include "UniqueIntegerStack.h"

namespace CGEngine {
	template <typename DomainKey, typename DomainValue, int MaxSize>
	class UniqueDomain {
	public:
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
	private:
		map<DomainKey, DomainValue> domain;
		UniqueIntegerStack<DomainKey> ids = UniqueIntegerStack<DomainKey>(MaxSize);
	};
}