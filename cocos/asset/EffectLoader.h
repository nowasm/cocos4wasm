#pragma once

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "core/assets/EffectAsset.h"

namespace cc {

// Resolves a `.effect` file to an EffectAsset.
//
// Scope: the Cocos Creator `.effect` compiled format is a rich, multi-pass
// shader blob normally consumed via `BuiltinEffectLoader` (which reads
// `templates/wasm32/builtin-effects.json` at boot). In the common case the
// effect an authored material references is already in `EffectAsset::get`'s
// global registry under its `_name` — so this loader looks up that name in
// the .effect file and returns the pre-registered asset.
//
// Full custom-effect parsing (techniques → passes → shader stages → GFX
// descriptors) is deferred until an authored project actually ships one.
class EffectLoader {
public:
    static EffectAsset *loadFromFile(const ccstd::string &absPath);
};

}  // namespace cc
