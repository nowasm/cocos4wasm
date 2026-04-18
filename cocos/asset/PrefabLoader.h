#pragma once

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "cocos/asset/Prefab.h"

namespace cc {

// Loads a Cocos Creator `.prefab` JSON file into a Prefab asset.
//
// Accepts both authoring shapes:
//   A. Wrapped (Creator export):
//      [ {"__type__":"cc.Prefab","data":{"__id__":1}},
//        {"__type__":"cc.Node", ...}, ... ]
//   B. Bare Node tree (hand-authored fixtures):
//      [ {"__type__":"cc.Node", ...}, ... ]
//
// When a wrapper is present the loader resolves `data.__id__` and stores it
// as the prefab's rootIndex; instantiate() then returns slot[rootIndex].
class PrefabLoader {
public:
    static Prefab *loadFromFile(const ccstd::string &absPath);
};

}  // namespace cc
