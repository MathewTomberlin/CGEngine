#pragma once

#include <map>
#include <set>
#include <optional>
#include <iostream>
using namespace std;

namespace CGEngine {
    template <typename IntType = id_t>
    class UniqueIntegerStack {
    public:
        UniqueIntegerStack(IntType count) {
            for (IntType i = 0; i < count; i++) {
                uniqueIds.insert(i);
            }
        };

        IntType receive(optional<IntType>* reciever) {
            if (uniqueIds.size() > 0) {
                IntType id = *uniqueIds.begin();
                uniqueIds.erase(id);
                *reciever = id;
                mappedIds[reciever] = id;
                removedIds.insert(id);
                return id;
            }
            return 0U;
        }

        IntType take() {
            if (uniqueIds.size() > 0) {
                IntType id = *uniqueIds.begin();
                uniqueIds.erase(id);
                removedIds.insert(id);
                return id;
            }
            return 0U;
        }

        void give(IntType key) {
            if (removedIds.find(key) != removedIds.end()) {
                uniqueIds.insert(key);
                removedIds.erase(key);
            }
        }

        void refund(optional<IntType>* reciever) {
            auto iter = mappedIds.find(reciever);
            if (iter != mappedIds.end()) {
                IntType id = (*iter).second;
                *reciever = nullopt;
                mappedIds.erase(reciever);
                removedIds.erase(id);
                uniqueIds.insert(id);
            }
        }
    private:
        set<IntType> uniqueIds;
        set<IntType> removedIds;
        map<optional<IntType>*, IntType> mappedIds;
    };
}