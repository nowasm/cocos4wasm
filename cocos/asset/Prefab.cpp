#include "cocos/asset/Prefab.h"

#include "base/Log.h"
#include "cocos/serialization/JsonDeserializer.h"
#include "core/scene-graph/Node.h"

namespace cc {

// Reflected so the JSON deserialiser can allocate a `cc.Prefab` slot 0
// when Creator-style wrapped .prefab files pass through the pipeline.
// The allocated instance is discarded (rootIndex > 0 selects the wrapped
// Node); this just silences the "unknown class" error.
CC_IMPLEMENT_ROOT_CLASS(Prefab, "cc.Prefab")
CC_END_CLASS(Prefab);

Node *Prefab::instantiate() const {
    if (_jsonText.empty()) {
        CC_LOG_ERROR("[Prefab] instantiate called on empty prefab");
        return nullptr;
    }
    serialization::JsonDeserializer d;
    Node *root = d.deserializeAs<Node>(_jsonText, _rootIndex);
    if (!root) {
        CC_LOG_ERROR("[Prefab] deserialize failed");
        return nullptr;
    }
    root->addRef();  // caller owns the returned pointer
    return root;
}

}  // namespace cc
