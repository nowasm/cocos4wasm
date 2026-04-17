#include "cocos/2d/framework/UITransform.h"

namespace cc {

CC_IMPLEMENT_CLASS(UITransform, "cc.UITransform", Component)
    .property("anchorPoint", &UITransform::_anchorPoint, Vec2{0.5f, 0.5f})
    .property("contentSize", &UITransform::_contentSize, Vec2{100.0f, 100.0f})
    .property("priority",    &UITransform::_priority)
CC_END_CLASS(UITransform);

}  // namespace cc
