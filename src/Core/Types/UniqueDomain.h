#pragma once
#include "Types.h"
#include <functional>
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
			domain[key] = std::move(value);
			return key;
		}

		void remove(DomainKey key) {
			if (domain.find(key) != domain.end()) {
				ids.give(key);
				domain.erase(key);
			}
		}

		const DomainValue& get(DomainKey key) {
			auto it = domain.find(key);
			if (it != domain.end()) {
				return it->second;
			} else {
				static DomainValue def;
				return def;
			}
		}

		bool has(DomainKey key) {
			return (domain.find(key) != domain.end());
		}

		void forEach(function<void(const DomainValue&)> function) {
			for (auto iterator = domain.begin(); iterator != domain.end(); ++iterator) {
				function(iterator->second);
			}
		}

		void clear() {
			domain.clear();
		}

		size_t size() {
			return domain.size();
		}
	private:
		map<DomainKey, DomainValue> domain;
		UniqueIntegerStack<DomainKey> ids = UniqueIntegerStack<DomainKey>(0U);
	};
}