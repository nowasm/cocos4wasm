// USDSkelScene — UsdSkel → engine GPU skinning end-to-end demo.
//
// Writes an inline USDA containing a two-joint skinned "arm" (two stacked
// boxes, 16 points): joint Base at the origin, joint Base/Upper at y=1.
// A SkelAnimation waves Upper ±45° around Z over 2 seconds. The loader
// turns this into a SkinningModel driven by an AnimationClip through
// cc.SkinnedMeshRenderer + cc.Animation components; this scene just
// activates the node tree and plays the clip in LOOP mode.

#ifdef USE_TINYUSDZ

#include "SceneRegistry.h"

#include "3d/framework/SkinnedMeshRendererComponent.h"
#include "animation/AnimationComponent.h"
#include "base/Log.h"
#include "cocos/game/USDLoader.h"
#include "core/component/NodeActivator.h"

#include <cstdio>

using namespace cc;
using namespace cc::game;

// ─── Inline UsdSkel USDA ─────────────────────────────────────────────────────

static bool writeSkelScene(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return false;

    fputs(
        "#usda 1.0\n"
        "(\n"
        "    upAxis = \"Y\"\n"
        "    metersPerUnit = 1\n"
        "    timeCodesPerSecond = 24\n"
        "    startTimeCode = 0\n"
        "    endTimeCode = 48\n"
        "    defaultPrim = \"Root\"\n"
        ")\n"
        "\n"
        "def Xform \"Root\"\n"
        "{\n"
        "    double3 xformOp:translate = (0, -1, 0)\n"
        "    uniform token[] xformOpOrder = [\"xformOp:translate\"]\n"
        "\n"
        "    def SkelRoot \"Arm\"\n"
        "    {\n"
        "        def Skeleton \"Skel\" (\n"
        "            prepend apiSchemas = [\"SkelBindingAPI\"]\n"
        "        )\n"
        "        {\n"
        "            uniform token[] joints = [\"Base\", \"Base/Upper\"]\n"
        "            uniform matrix4d[] bindTransforms = [\n"
        "                ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1)),\n"
        "                ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 1, 0, 1))\n"
        "            ]\n"
        "            uniform matrix4d[] restTransforms = [\n"
        "                ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1)),\n"
        "                ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 1, 0, 1))\n"
        "            ]\n"
        "            rel skel:animationSource = </Root/Arm/Skel/Anim>\n"
        "\n"
        "            def SkelAnimation \"Anim\"\n"
        "            {\n"
        "                uniform token[] joints = [\"Base\", \"Base/Upper\"]\n"
        "                float3[] translations.timeSamples = {\n"
        "                    0: [(0, 0, 0), (0, 1, 0)],\n"
        "                    24: [(0, 0, 0), (0, 1, 0)],\n"
        "                    48: [(0, 0, 0), (0, 1, 0)]\n"
        "                }\n"
        // USDA quat literals are (w, x, y, z): rotation about Z by ±45°.
        "                quatf[] rotations.timeSamples = {\n"
        "                    0: [(1, 0, 0, 0), (0.9238795, 0, 0, 0.3826834)],\n"
        "                    24: [(1, 0, 0, 0), (0.9238795, 0, 0, -0.3826834)],\n"
        "                    48: [(1, 0, 0, 0), (0.9238795, 0, 0, 0.3826834)]\n"
        "                }\n"
        "                half3[] scales.timeSamples = {\n"
        "                    0: [(1, 1, 1), (1, 1, 1)],\n"
        "                    48: [(1, 1, 1), (1, 1, 1)]\n"
        "                }\n"
        "            }\n"
        "        }\n"
        "\n"
        "        def Mesh \"ArmMesh\" (\n"
        "            prepend apiSchemas = [\"SkelBindingAPI\"]\n"
        "        )\n"
        "        {\n"
        "            rel skel:skeleton = </Root/Arm/Skel>\n"
        "            color3f[] primvars:displayColor = [(0.85, 0.55, 0.15)] (interpolation = \"constant\")\n"
        "            int[] faceVertexCounts = [4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4]\n"
        "            int[] faceVertexIndices = [\n"
        "                0,1,2,3, 4,5,6,7, 0,4,7,3, 1,5,6,2, 0,1,5,4, 3,2,6,7,\n"
        "                8,9,10,11, 12,13,14,15, 8,12,15,11, 9,13,14,10, 8,9,13,12, 11,10,14,15\n"
        "            ]\n"
        "            point3f[] points = [\n"
        "                (-0.25, 0, 0.25), (0.25, 0, 0.25), (0.25, 1, 0.25), (-0.25, 1, 0.25),\n"
        "                (-0.25, 0, -0.25), (0.25, 0, -0.25), (0.25, 1, -0.25), (-0.25, 1, -0.25),\n"
        "                (-0.25, 1, 0.25), (0.25, 1, 0.25), (0.25, 2, 0.25), (-0.25, 2, 0.25),\n"
        "                (-0.25, 1, -0.25), (0.25, 1, -0.25), (0.25, 2, -0.25), (-0.25, 2, -0.25)\n"
        "            ]\n"
        "            normal3f[] normals = [\n"
        "                (0,0,1),(0,0,1),(0,0,1),(0,0,1),\n"
        "                (0,0,-1),(0,0,-1),(0,0,-1),(0,0,-1),\n"
        "                (-1,0,0),(-1,0,0),(-1,0,0),(-1,0,0),\n"
        "                (1,0,0),(1,0,0),(1,0,0),(1,0,0),\n"
        "                (0,-1,0),(0,-1,0),(0,-1,0),(0,-1,0),\n"
        "                (0,1,0),(0,1,0),(0,1,0),(0,1,0),\n"
        "                (0,0,1),(0,0,1),(0,0,1),(0,0,1),\n"
        "                (0,0,-1),(0,0,-1),(0,0,-1),(0,0,-1),\n"
        "                (-1,0,0),(-1,0,0),(-1,0,0),(-1,0,0),\n"
        "                (1,0,0),(1,0,0),(1,0,0),(1,0,0),\n"
        "                (0,-1,0),(0,-1,0),(0,-1,0),(0,-1,0),\n"
        "                (0,1,0),(0,1,0),(0,1,0),(0,1,0)\n"
        "            ] (interpolation = \"faceVarying\")\n"
        "            int[] primvars:skel:jointIndices = [\n"
        "                0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1\n"
        "            ] (\n"
        "                elementSize = 1\n"
        "                interpolation = \"vertex\"\n"
        "            )\n"
        "            float[] primvars:skel:jointWeights = [\n"
        "                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1\n"
        "            ] (\n"
        "                elementSize = 1\n"
        "                interpolation = \"vertex\"\n"
        "            )\n"
        "        }\n"
        "    }\n"
        "}\n",
        f);

    fclose(f);
    return true;
}

// ─── USDSkelScene ────────────────────────────────────────────────────────────

class USDSkelScene : public DemoScene {
public:
    const char* name() const override { return "USDSkelScene"; }

    void onEnter(scene::RenderScene*, Root*) override {
        const char* path = "usd_skel_scene.usda";
        if (!writeSkelScene(path)) {
            CC_LOG_ERROR("[USDSkelScene] failed to write inline USDA");
            return;
        }

        _result = USDLoader::load(path, nullptr);
        if (!_result.success) {
            CC_LOG_ERROR("[USDSkelScene] load failed: %s", _result.error.c_str());
            return;
        }

        CC_LOG_INFO("[USDSkelScene] loaded: %zu skinned renderer(s), %zu clip(s), %zu skeleton(s)",
                    _result.skinnedRenderers.size(),
                    _result.animationClips.size(),
                    _result.skeletons.size());

        // Activate the loaded tree so components run their lifecycle
        // (SkinnedMeshRenderer attaches its SkinningModel to the scene).
        NodeActivator::get().activateNode(_result.rootNode, true);

        // Play every skeleton clip in LOOP.
        for (auto* animComp : _result.animationComponents) {
            auto* state = animComp->play();
            if (state) state->wrapMode = AnimationWrapMode::LOOP;
        }
    }

    void onUpdate(float dt) override {
        if (!_result.success) return;
        _t += dt;
        // One-shot evidence log: after ~0.6 s the Upper joint must have left
        // its rest pose if the clip is driving the joint tree.
        if (!_logged && _t > 0.6f) {
            _logged = true;
            if (!_result.skinnedRenderers.empty()) {
                Node* root  = _result.skinnedRenderers[0]->getSkinningRoot();
                Node* upper = root ? root->getChildByPath("Base/Upper") : nullptr;
                if (upper) {
                    const Quaternion& q = upper->getRotation();
                    CC_LOG_INFO("[USDSkelScene] t=%.2f Upper joint quat=(%.3f, %.3f, %.3f, %.3f)%s",
                                _t, q.x, q.y, q.z, q.w,
                                std::fabs(q.z) > 0.01f ? " — animating OK" : " — NOT animating");
                } else {
                    CC_LOG_ERROR("[USDSkelScene] joint node 'Base/Upper' not found");
                }
            }
        }
    }

    void onExit() override {
        if (_result.rootNode) {
            NodeActivator::get().activateNode(_result.rootNode, false);
        }
        _result.destroy();
        _t = 0;
        _logged = false;
        CC_LOG_INFO("[USDSkelScene] exit");
    }

private:
    USDLoadResult _result;
    float _t{0};
    bool  _logged{false};
};

REGISTER_DEMO_SCENE("USDSkelScene", USDSkelScene);

#endif // USE_TINYUSDZ
