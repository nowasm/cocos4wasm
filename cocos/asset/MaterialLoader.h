#pragma once

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "core/assets/Material.h"

namespace cc {

// Loads Cocos Creator 3.x-format .mtl JSON into a Material asset.
//
// Expected JSON shape (top-level array with one or more objects; the first
// entry tagged "__type__":"cc.Material" is the material itself):
//
//   [
//     {
//       "__type__":   "cc.Material",
//       "_effectAsset": { "__uuid__": "..." } | "<builtin-name>",
//       "_techIdx":   0,
//       "_defines":   [ { "USE_TEXTURE": true, "CC_SOME_INT": 1 } ],
//       "_props":     [ { "mainTexture": {"__uuid__":"..."},
//                         "mainColor":   { "__type__":"cc.Color","r":255,...} } ]
//     }
//   ]
//
// Resolving:
//   - `_effectAsset` UUID → AssetManager.loadAsset (falls back to
//      EffectAsset::get(name) if a raw string is given, e.g. "builtin-unlit")
//   - `_props[pass][name]` → material->setProperty(...) post-initialize
//
// Skipped for MVP: `_states` (pass overrides), variant-rich numeric types
// (Quat/Mat3/Mat4), per-pass macro arrays. Extend here when needed.
class MaterialLoader {
public:
    // Reads the file at `absPath`, parses the JSON, and returns a fully
    // initialised Material. Returns nullptr on IO / parse / resolution
    // failure (errors logged).
    static Material *loadFromFile(const ccstd::string &absPath);
};

}  // namespace cc
