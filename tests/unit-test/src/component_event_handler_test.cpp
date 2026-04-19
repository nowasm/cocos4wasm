#include "base/Ptr.h"
#include "core/component/Component.h"
#include "core/scene-graph/ComponentEventHandler.h"
#include "core/scene-graph/Node.h"
#include "cocos/ui/components/Button.h"
#include "gtest/gtest.h"

using namespace cc;
using namespace cc::reflection;

// ─── Test component with reflected handler methods ────────────────────────
// "ceh_test." prefix keeps these out of the real engine class namespace so
// parallel test runs don't collide on ClassDB registrations.

namespace {

class TargetComp : public Component {
    CC_CLASS_DECL(TargetComp, Component)
public:
    int _clickCount{0};
    ccstd::string _lastData;
    Component *_lastSender{nullptr};

    // Typical upstream handler shape: (sender, customData).
    void onClick(Component *sender, const ccstd::string &data) {
        ++_clickCount;
        _lastSender = sender;
        _lastData = data;
    }

    // No-arg variant — some handlers ignore context entirely.
    void onSilent() {
        ++_clickCount;
    }

    // String-only — customEventData-driven dispatch with no sender param.
    void onCustom(const ccstd::string &data) {
        _lastData = data;
    }
};

}  // namespace

CC_IMPLEMENT_CLASS(TargetComp, "ceh_test.TargetComp", Component)
    .method("onClick",  &TargetComp::onClick)
    .method("onSilent", &TargetComp::onSilent)
    .method("onCustom", &TargetComp::onCustom)
CC_END_CLASS(TargetComp);

// ─── Tests ────────────────────────────────────────────────────────────────
//
// RefCounted starts at count 0 — fresh `ccnew Node()` / `ccnew ComponentEventHandler()`
// has refcount 0, so tests wrap them in IntrusivePtr which addRef-on-assign.
// Matching convention used by Prefab::instantiate() and the scene graph.

TEST(ComponentEventHandler, ClassRegistered) {
    auto *meta = ComponentEventHandler::getStaticClass();
    ASSERT_NE(meta, nullptr);
    EXPECT_STREQ(meta->name, "cc.ClickEvent");

    // Fields matching upstream `@serializable` declarations
    EXPECT_NE(meta->findProperty("target"), nullptr);
    EXPECT_NE(meta->findProperty("component"), nullptr);
    EXPECT_NE(meta->findProperty("_componentId"), nullptr);
    EXPECT_NE(meta->findProperty("handler"), nullptr);
    EXPECT_NE(meta->findProperty("customEventData"), nullptr);
}

namespace {

// Helper: wires up a freshly allocated Node + TargetComp with proper refcounts.
// Returns a pair of IntrusivePtrs so the caller doesn't need to touch addRef.
struct Fixture {
    IntrusivePtr<Node> node;
    TargetComp *comp{nullptr};   // owned by node->_components
};

Fixture makeFixture() {
    Fixture fx;
    fx.node = ccnew Node();
    fx.comp = fx.node->addComponent<TargetComp>();
    return fx;
}

}  // namespace

TEST(ComponentEventHandler, EmitReachesTargetMethod) {
    auto fx = makeFixture();
    IntrusivePtr<ComponentEventHandler> h = ccnew ComponentEventHandler();
    h->target = fx.node.get();
    h->component = "ceh_test.TargetComp";
    h->handler = "onClick";

    MethodArgs args;
    args.push_back(MethodArg::makePointer(fx.comp));
    h->emit(args);

    EXPECT_EQ(fx.comp->_clickCount, 1);
    EXPECT_EQ(fx.comp->_lastSender, fx.comp);
}

TEST(ComponentEventHandler, CustomEventDataAppendedAsStringArg) {
    auto fx = makeFixture();
    IntrusivePtr<ComponentEventHandler> h = ccnew ComponentEventHandler();
    h->target = fx.node.get();
    h->component = "ceh_test.TargetComp";
    h->handler = "onClick";
    h->customEventData = "reward_x2";

    MethodArgs args;
    args.push_back(MethodArg::makePointer(fx.comp));
    h->emit(args);

    EXPECT_EQ(fx.comp->_clickCount, 1);
    EXPECT_EQ(fx.comp->_lastData, "reward_x2");
}

TEST(ComponentEventHandler, EmitWithZeroArgsStillFindsHandler) {
    auto fx = makeFixture();
    IntrusivePtr<ComponentEventHandler> h = ccnew ComponentEventHandler();
    h->target = fx.node.get();
    h->component = "ceh_test.TargetComp";
    h->handler = "onSilent";

    MethodArgs args;
    h->emit(args);
    EXPECT_EQ(fx.comp->_clickCount, 1);
}

TEST(ComponentEventHandler, EmitOnlyStringCustomData) {
    // Handler signature: void(const string&) — only customEventData matters.
    auto fx = makeFixture();
    IntrusivePtr<ComponentEventHandler> h = ccnew ComponentEventHandler();
    h->target = fx.node.get();
    h->component = "ceh_test.TargetComp";
    h->handler = "onCustom";
    h->customEventData = "ping";

    MethodArgs noPositional;
    h->emit(noPositional);
    EXPECT_EQ(fx.comp->_lastData, "ping");
}

TEST(ComponentEventHandler, NullTargetIsNoOp) {
    IntrusivePtr<ComponentEventHandler> h = ccnew ComponentEventHandler();
    h->target = nullptr;
    h->component = "ceh_test.TargetComp";
    h->handler = "onClick";
    MethodArgs args;
    EXPECT_NO_FATAL_FAILURE(h->emit(args));
}

TEST(ComponentEventHandler, EmptyHandlerNameIsNoOp) {
    auto fx = makeFixture();
    IntrusivePtr<ComponentEventHandler> h = ccnew ComponentEventHandler();
    h->target = fx.node.get();
    h->component = "ceh_test.TargetComp";
    h->handler = "";
    MethodArgs args;
    EXPECT_NO_FATAL_FAILURE(h->emit(args));
}

TEST(ComponentEventHandler, UnknownComponentNameIsNoOp) {
    auto fx = makeFixture();
    IntrusivePtr<ComponentEventHandler> h = ccnew ComponentEventHandler();
    h->target = fx.node.get();
    h->component = "ceh_test.NotARealClass";
    h->handler = "onClick";
    MethodArgs args;
    EXPECT_NO_FATAL_FAILURE(h->emit(args));
}

TEST(ComponentEventHandler, ComponentIdFallbackWhenComponentEmpty) {
    // When `component` is empty, the emit path falls back to `_componentId` —
    // upstream uses this for script components identified by numeric UUID.
    auto fx = makeFixture();
    IntrusivePtr<ComponentEventHandler> h = ccnew ComponentEventHandler();
    h->target = fx.node.get();
    h->component = "";
    h->_componentId = "ceh_test.TargetComp";
    h->handler = "onSilent";

    MethodArgs args;
    h->emit(args);
    EXPECT_EQ(fx.comp->_clickCount, 1);
}

TEST(ComponentEventHandler, EmitEventsFanOut) {
    auto fx = makeFixture();
    ccstd::vector<IntrusivePtr<ComponentEventHandler>> events;
    for (int i = 0; i < 3; ++i) {
        auto *h = ccnew ComponentEventHandler();
        h->target = fx.node.get();
        h->component = "ceh_test.TargetComp";
        h->handler = "onSilent";
        events.emplace_back(h);
    }

    MethodArgs args;
    ComponentEventHandler::emitEvents(events, args);
    EXPECT_EQ(fx.comp->_clickCount, 3);
}

// ─── Button integration: reflected clickEvents + dispatch wiring ──────────

TEST(ComponentEventHandler, ButtonRegistersClickEventsArray) {
    auto *meta = Button::getStaticClass();
    ASSERT_NE(meta, nullptr);

    // Property name matches upstream button.ts exactly — no leading
    // underscore. Editor JSON writes it as "clickEvents".
    auto *p = meta->findProperty("clickEvents");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->typeId, reflection::TypeId::ARRAY);
    EXPECT_EQ(p->elementTypeId, reflection::TypeId::POINTER);
    ASSERT_NE(p->pointeeClassFn, nullptr);
    EXPECT_STREQ(p->pointeeClassFn()->name, "cc.ClickEvent");
}

TEST(ComponentEventHandler, ButtonClickEventsArrayAppendRoundtrip) {
    // Simulates what the JSON deserializer does: it grabs the property's
    // array accessors (arrayClear + arrayAppend) and pushes each resolved
    // handler pointer. We verify the push actually lands in Button's field.
    IntrusivePtr<Node> node = ccnew Node();
    auto *btn = node->addComponent<Button>();

    auto *meta = Button::getStaticClass();
    auto *p = meta->findProperty("clickEvents");
    ASSERT_NE(p, nullptr);

    IntrusivePtr<ComponentEventHandler> h = ccnew ComponentEventHandler();
    h->handler = "onSilent";
    ComponentEventHandler *raw = h.get();

    p->arrayClear(btn);
    p->arrayAppend(btn, &raw);

    ASSERT_EQ(btn->getClickEvents().size(), 1u);
    EXPECT_EQ(btn->getClickEvents()[0].get(), raw);
}

TEST(ComponentEventHandler, NullEntriesInArrayAreSkipped) {
    auto fx = makeFixture();
    ccstd::vector<IntrusivePtr<ComponentEventHandler>> events;
    events.emplace_back(nullptr);
    auto *h = ccnew ComponentEventHandler();
    h->target = fx.node.get();
    h->component = "ceh_test.TargetComp";
    h->handler = "onSilent";
    events.emplace_back(h);
    events.emplace_back(nullptr);

    MethodArgs args;
    ComponentEventHandler::emitEvents(events, args);
    EXPECT_EQ(fx.comp->_clickCount, 1);
}
