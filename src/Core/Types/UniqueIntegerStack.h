#pragma once

#include <map>
#include <set>
#include <optional>
#include <iostream>
#include "../Types/Types.h"

namespace CGEngine {
    template <typename IntType = id_t>
    class UniqueIntegerStack {
    public:
        // Ids 0..count-1 are available. They are not stored up front: every id at or above nextUnused has never been
        // handed out, so only returned ids need a set. (Filling a set with 1000 ids per stack cost about 1 ms per Body.)
        UniqueIntegerStack(IntType count) : count(count) {};

        IntType receive(optional<IntType>* reciever) {
            if (optional<IntType> id = next()) {
                *reciever = id.value();
                mappedIds[reciever] = id.value();
                return id.value();
            }
            return 0U;
        }

        IntType take() {
            return next().value_or(0U);
        }

        void give(IntType key) {
            if (removedIds.find(key) != removedIds.end()) {
                returnedIds.insert(key);
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
                returnedIds.insert(id);
            }
        }
    private:
        // The smallest available id: a returned one (all are below nextUnused), else the next never-used one.
        optional<IntType> next() {
            IntType id;
            if (!returnedIds.empty()) {
                id = *returnedIds.begin();
                returnedIds.erase(returnedIds.begin());
            } else if (nextUnused < count) {
                id = nextUnused++;
            } else {
                return nullopt;
            }
            removedIds.insert(id);
            return id;
        }

        IntType count = 0;
        IntType nextUnused = 0;
        set<IntType> returnedIds;
        set<IntType> removedIds;
        map<optional<IntType>*, IntType> mappedIds;
    };
}