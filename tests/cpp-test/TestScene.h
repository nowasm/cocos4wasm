#pragma once
#include <cmath>
#include <functional>
#include <string>
#include <vector>
#include "cocos/game/CocosGame.h"

// ---- Base class for all test scenes ----
class TestScene {
public:
    virtual ~TestScene() = default;
    virtual std::string name() = 0;
    virtual std::string category() = 0;
    virtual void setup(cc::scene::RenderScene *rs, cc::Root *root) = 0;
    virtual void update(float dt) {}
    virtual void cleanup() {
        for (auto *r : _renderers) delete r;
        _renderers.clear();
        for (auto *n : _nodes) n->release();
        _nodes.clear();
    }

protected:
    // helpers — track allocations for cleanup
    cc::Node *createNode(const std::string &name) {
        auto *n = ccnew cc::Node(name);
        _nodes.push_back(n);
        return n;
    }
    cc::game::MeshRenderer *addMeshRenderer(cc::Node *node,
                                            cc::Mesh *mesh,
                                            cc::Material *mat) {
        auto *r = ccnew cc::game::MeshRenderer(node);
        r->setMesh(mesh);
        if (mat) r->setMaterial(mat);
        _renderers.push_back(r);
        return r;
    }

    std::vector<cc::game::MeshRenderer *> _renderers;
    std::vector<cc::Node *> _nodes;
};

// ---- Global test registry ----
using TestFactory = std::function<TestScene *()>;

struct TestEntry {
    std::string category;
    std::string name;
    TestFactory factory;
};

inline std::vector<TestEntry> &getTestRegistry() {
    static std::vector<TestEntry> reg;
    return reg;
}

struct TestRegistrar {
    TestRegistrar(const char *cat, const char *name, TestFactory f) {
        getTestRegistry().push_back({cat, name, std::move(f)});
    }
};

#define REGISTER_TEST(Category, Name, Class) \
    static TestRegistrar _reg_##Class(Category, Name, [] { return new Class(); })
