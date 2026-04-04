/****************************************************************************
 Copyright (c) 2017-2023 Xiamen Yaji Software Co., Ltd.

 http://www.cocos.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
****************************************************************************/

#include "engine/Engine.h"
#include <cstdlib>
#include <functional>
#include <memory>
#include <sstream>
#include "base/DeferredReleasePool.h"
#include "base/Macros.h"
#include "bindings/jswrapper/SeApi.h"
#include "core/Root.h"
#include "core/builtin/BuiltinResMgr.h"
#include "scene/Camera.h"
#include "scene/RenderScene.h"
#if CC_PLATFORM == CC_PLATFORM_EMSCRIPTEN
#include "2d/renderer/Batcher2d.h"
#endif
#include "engine/EngineEvents.h"
#include "platform/BasePlatform.h"
#include "platform/FileUtils.h"
#include "renderer/GFXDeviceManager.h"
#include "renderer/core/ProgramLib.h"
#include "renderer/pipeline/RenderPipeline.h"
#include "renderer/pipeline/custom/RenderingModule.h"

#if CC_USE_AUDIO
    #include "cocos/audio/include/AudioEngine.h"
#endif

#if CC_USE_SOCKET
    #include "cocos/network/WebSocket.h"
#endif

#if CC_USE_DRAGONBONES
    #include "editor-support/dragonbones-creator-support/ArmatureCacheMgr.h"
#endif

#if CC_USE_SPINE
    #include "editor-support/spine-creator-support/SkeletonCacheMgr.h"
#endif

#include "application/ApplicationManager.h"
#include "application/BaseApplication.h"
#include "base/Scheduler.h"
#include "bindings/event/EventDispatcher.h"
#include "network/HttpClient.h"
#include "platform/UniversalPlatform.h"
#include "platform/interfaces/modules/IScreen.h"
#include "platform/interfaces/modules/ISystemWindow.h"
#include "platform/interfaces/modules/ISystemWindowManager.h"
#if CC_USE_DEBUG_RENDERER
    #include "core/assets/FreeTypeFont.h"
    #include "profiler/DebugRenderer.h"
#endif
#include "profiler/Profiler.h"

namespace {

bool setCanvasCallback(se::Object *global) {
    const se::AutoHandleScope scope;
    se::ScriptEngine *se = se::ScriptEngine::getInstance();
    auto *window = CC_GET_MAIN_SYSTEM_WINDOW();
    auto handler = window->getWindowHandle();
    auto viewSize = window->getViewSize();
    auto dpr = cc::BasePlatform::getPlatform()->getInterface<cc::IScreen>()->getDevicePixelRatio();

    se::Value jsbVal;
    const bool ok = global->getProperty("jsb", &jsbVal);
    if (!jsbVal.isObject()) {
        const se::HandleObject jsbObj(se::Object::createPlainObject());
        global->setProperty("jsb", se::Value(jsbObj));
        jsbVal.setObject(jsbObj, true);
    }

    se::Value windowVal;
    jsbVal.toObject()->getProperty("window", &windowVal);
    if (!windowVal.isObject()) {
        const se::HandleObject windowObj(se::Object::createPlainObject());
        jsbVal.toObject()->setProperty("window", se::Value(windowObj));
        windowVal.setObject(windowObj, true);
    }

    const int width = static_cast<int>(viewSize.width / dpr);
    const int height = static_cast<int>(viewSize.height / dpr);
    windowVal.toObject()->setProperty("innerWidth", se::Value(width));
    windowVal.toObject()->setProperty("innerHeight", se::Value(height));

    if (sizeof(handler) == 8) { // use bigint
        windowVal.toObject()->setProperty("windowHandle", se::Value(static_cast<uint64_t>(handler)));
    } else {
        windowVal.toObject()->setProperty("windowHandle", se::Value(static_cast<uint32_t>(handler)));
    }

    return true;
}

} // namespace

namespace cc {

/** static */
bool Engine::isValid() {
    return CC_CURRENT_APPLICATION() 
        && CC_CURRENT_ENGINE() 
        && se::ScriptEngine::getInstance()
        && se::ScriptEngine::getInstance()->isValid();
}

Engine::Engine() {
    _scriptEngine = ccnew se::ScriptEngine();

    _windowEventListener.bind([this](const cc::WindowEvent &ev) { redirectWindowEvent(ev); });
}

Engine::~Engine() {
    destroy();

    delete _scriptEngine;
    _scriptEngine = nullptr;
}

int32_t Engine::init() {
    _scheduler = std::make_shared<Scheduler>();
    _fs = createFileUtils();
    // May create gfx device in render subsystem in future.
    _gfxDevice = gfx::DeviceManager::create();
    _programLib = ccnew ProgramLib();
    _builtinResMgr = ccnew BuiltinResMgr;

#if CC_USE_DEBUG_RENDERER
    _debugRenderer = ccnew DebugRenderer();
#endif

#if CC_USE_PROFILER
    _profiler = ccnew Profiler();
#endif

    EventDispatcher::init();

    BasePlatform *platform = BasePlatform::getPlatform();

    se::ScriptEngine::getInstance()->addRegisterCallback(setCanvasCallback);
    emit<EngineStatusChange>(ON_START);
    _inited = true;
    return 0;
}

void Engine::destroy() {
    cc::DeferredReleasePool::clear();
    cc::network::HttpClient::destroyInstance();
    _scheduler->removeAllFunctionsToBePerformedInCocosThread();
    _scheduler->unscheduleAll();
    CCObject::deferredDestroy();

#if CC_USE_AUDIO
    AudioEngine::end();
#endif

    EventDispatcher::destroy();

    // Should delete it before deleting DeviceManager as ScriptEngine will check gpu resource usage,
    // and ScriptEngine will hold gfx objects.
    // Because the user registration interface needs to be added during initialization.
    // ScriptEngine cannot be released here.
    _scriptEngine->cleanup();

#if CC_USE_PROFILER
    delete _profiler;
#endif
    // Profiler depends on DebugRenderer, should delete it after deleting Profiler,
    // and delete DebugRenderer after RenderPipeline::destroy which destroy DebugRenderer.
#if CC_USE_DEBUG_RENDERER
    delete _debugRenderer;
#endif

    // TODO(): Delete some global objects.
#if CC_USE_DEBUG_RENDERER
    // FreeTypeFontFace is only used in DebugRenderer now, so use CC_USE_DEBUG_RENDERER macro temporarily
    FreeTypeFontFace::destroyFreeType();
#endif

#if CC_USE_DRAGONBONES
    dragonBones::ArmatureCacheMgr::destroyInstance();
#endif

#if CC_USE_SPINE
    cc::SkeletonCacheMgr::destroyInstance();
#endif

#if CC_USE_MIDDLEWARE
    cc::middleware::MiddlewareManager::destroyInstance();
#endif

    CCObject::deferredDestroy();

    delete _builtinResMgr;
    delete _programLib;

    if (cc::render::getRenderingModule()) {
        cc::render::Factory::destroy(cc::render::getRenderingModule());
    }
    #if (SCRIPT_ENGINE_TYPE == SCRIPT_ENGINE_JSVM)
        // When using JSVM, not all objects are destroyed during cleanup, so we need to close JSVM at the end.
        _scriptEngine->closeEngine();
    #endif

    CC_SAFE_DESTROY_AND_DELETE(_gfxDevice);
    delete _fs;
    _scheduler.reset();

    _inited = false;
}

int32_t Engine::run() {
    BasePlatform *platform = BasePlatform::getPlatform();
    _xr = CC_GET_XR_INTERFACE();
    platform->runInPlatformThread([&]() {
        tick();
    });
    return 0;
}

void Engine::pause() {
    // TODO(cc) : Follow-up support
}

void Engine::resume() {
    // TODO(cc) : Follow-up support
}

int Engine::restart() {
    _needRestart = true;
    return 0;
}

void Engine::close() { // NOLINT

#if CC_USE_AUDIO
    cc::AudioEngine::stopAll();
#endif

    // #if CC_USE_SOCKET
    //     cc::network::WebSocket::closeAllConnections();
    // #endif

    cc::DeferredReleasePool::clear();
    _scheduler->removeAllFunctionsToBePerformedInCocosThread();
    _scheduler->unscheduleAll();
}

uint Engine::getTotalFrames() const {
    return _totalFrames;
}

void Engine::setPreferredFramesPerSecond(int fps) {
    if (fps == 0) {
        return;
    }
    BasePlatform *platform = BasePlatform::getPlatform();
    platform->setFps(fps);
    _preferredNanosecondsPerFrame = static_cast<long>(1.0 / fps * NANOSECONDS_PER_SECOND); // NOLINT(google-runtime-int)
}

#if CC_PLATFORM == CC_PLATFORM_EMSCRIPTEN
namespace {

static bool sBatcher2dRootSynced = false;

// Sync all Scene children as root nodes for Batcher2D rendering.
// Must be called after a scene has been activated and has children.
void syncBatcher2DRootNodes(Root *root) {
    auto *batcher = root->getBatcher2D();
    if (!batcher) {
        CC_LOG_WARNING("syncBatcher2D: batcher is null");
        return;
    }

    // Use the ScriptEngine to get __ccCurrentScene and its children from JS
    auto *seInst = se::ScriptEngine::getInstance();
    se::Value sceneVal;
    seInst->getGlobalObject()->getProperty("__ccCurrentScene", &sceneVal);
    if (!sceneVal.isObject()) {
        CC_LOG_WARNING("syncBatcher2D: __ccCurrentScene not set");
        return;
    }

    auto *seObj = sceneVal.toObject();
    void *rawPtr = seObj->getPrivateData();
    if (!rawPtr) {
        // Try getPrivateObject path
        auto *privObj = seObj->getPrivateObject();
        if (privObj) {
            rawPtr = privObj->getRaw();
        }
    }
    auto *sceneNode = static_cast<Node *>(rawPtr);
    if (!sceneNode) {
        CC_LOG_WARNING("syncBatcher2D: scene has no native Node (privateData=%p, privateObject=%p)",
                       seObj->getPrivateData(), static_cast<void*>(seObj->getPrivateObject()));
        return;
    }

    auto &children = sceneNode->getChildren();
    ccstd::vector<Node *> rootNodes;
    for (auto &child : children) {
        rootNodes.push_back(child);
    }

    if (!rootNodes.empty()) {
        batcher->syncRootNodesToNative(std::move(rootNodes));
        sBatcher2dRootSynced = true;
        CC_LOG_INFO("Engine: synced %d root nodes to Batcher2D", static_cast<int>(children.size()));
    } else {
        CC_LOG_WARNING("syncBatcher2D: scene has no children");
    }
}

// Automatically attach a default ortho camera to any RenderScene that
// has no cameras.  Called once per scene (the flag is the camera itself).
void ensureDefaultCameraOnScenes(Root *root) {
    auto *mainWindow = root->getMainWindow();
    if (!mainWindow) return;

    for (auto &rs : root->getScenes()) {
        if (!rs) continue;
        const auto &cams = rs->getCameras();
        if (!cams.empty()) continue;

        // This scene has no camera — create one
        auto *camNode = ccnew Node("DefaultCamera");
        camNode->setPosition(cc::Vec3(0, 0, 1000));

        auto *camera = root->createCamera();
        scene::ICameraInfo camInfo;
        camInfo.name = "DefaultCamera";
        camInfo.node = camNode;
        camInfo.projection = scene::CameraProjection::ORTHO;
        camInfo.window = mainWindow;
        camera->initialize(camInfo);
        float halfH = static_cast<float>(mainWindow->getHeight()) * 0.5F;
        camera->setOrthoHeight(halfH > 0 ? halfH : 320.F);
        camera->setNearClip(0.1F);
        camera->setFarClip(2000.F);
        camera->setVisibility(0xFFFFFFFF);
        camera->setClearFlag(gfx::ClearFlagBit::ALL);
        camera->setClearColor(gfx::Color{0.2F, 0.2F, 0.2F, 1.0F});
        rs->addCamera(camera);
        CC_LOG_INFO("Engine: auto-attached camera to RenderScene '%s'", rs->getName().c_str());
    }
}
} // namespace
#endif

void Engine::tick() {
    CC_PROFILER_BEGIN_FRAME;
    {
        CC_PROFILE(EngineTick);

        _gfxDevice->frameSync();

        if (_needRestart) {
            doRestart();
            _needRestart = false;
        }

        static std::chrono::steady_clock::time_point prevTime;
        static std::chrono::steady_clock::time_point now;
        static float dt = 0.F;
        static double dtNS = NANOSECONDS_60FPS;

        ++_totalFrames;

        // iOS/macOS use its own fps limitation algorithm.
        // Windows for Editor should not sleep,because Editor call tick function synchronously
#if (CC_PLATFORM == CC_PLATFORM_ANDROID || (CC_PLATFORM == CC_PLATFORM_WINDOWS && !CC_EDITOR) || CC_PLATFORM == CC_PLATFORM_OHOS || CC_PLATFORM == CC_PLATFORM_OPENHARMONY || CC_PLATFORM == CC_PLATFORM_MACOS)
        if (dtNS < static_cast<double>(_preferredNanosecondsPerFrame)) {
            CC_PROFILE(EngineSleep);
            std::this_thread::sleep_for(
                std::chrono::nanoseconds(_preferredNanosecondsPerFrame - static_cast<int64_t>(dtNS)));
            dtNS = static_cast<double>(_preferredNanosecondsPerFrame);
        }
#endif

        events::BeforeTick::broadcast();

        prevTime = std::chrono::steady_clock::now();
        if (_xr) _xr->beginRenderFrame();
        _scheduler->update(dt);

        se::ScriptEngine::getInstance()->handlePromiseExceptions();
        events::Tick::broadcast(dt);

        // Drive Root::frameMove from C++ directly.  In Creator builds this is
        // done from JS (gameTick -> root.frameMove), but frameMove has no JSB
        // binding so standalone WASM projects need this native call path.
        if (auto *root = Root::getInstance()) {
#if CC_PLATFORM == CC_PLATFORM_EMSCRIPTEN
            // Auto-attach a default camera to any RenderScene that has none.
            ensureDefaultCameraOnScenes(root);
            // Sync scene children to Batcher2D for 2D UI rendering.
            if (!sBatcher2dRootSynced) {
                syncBatcher2DRootNodes(root);
            }
#endif
            root->frameMove(dt, static_cast<int32_t>(_totalFrames));
        }

        se::ScriptEngine::getInstance()->mainLoopUpdate();

        cc::DeferredReleasePool::clear();
        if (_xr) _xr->endRenderFrame();
        
        // Executing async tasks at the end of the current frame to make the callback invoked as soon as possible.
        _scheduler->runFunctionsToBePerformedInCocosThread();
        
        now = std::chrono::steady_clock::now();
        dtNS = dtNS * 0.1 + 0.9 * static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - prevTime).count());
        dt = static_cast<float>(dtNS) / NANOSECONDS_PER_SECOND;

        events::AfterTick::broadcast();
    }

    CC_PROFILER_END_FRAME;
}

void Engine::doRestart() {
    events::RestartVM::broadcast();
    destroy();
    CC_CURRENT_APPLICATION()->init();
}

Engine::SchedulerPtr Engine::getScheduler() const {
    return _scheduler;
}

bool Engine::redirectWindowEvent(const WindowEvent &ev) {
    bool isHandled = false;

    switch (ev.type) {
        case WindowEvent::Type::SHOW:
        case WindowEvent::Type::RESTORED:
            emit<EngineStatusChange>(ON_RESUME);
#if CC_PLATFORM == CC_PLATFORM_WINDOWS
            events::WindowRecreated::broadcast(ev.windowId);
#endif
            events::EnterForeground::broadcast();
            isHandled = true;
            break;
        case WindowEvent::Type::SIZE_CHANGED:
        case WindowEvent::Type::RESIZED: {
            auto *w = CC_GET_SYSTEM_WINDOW(ev.windowId);
            CC_ASSERT(w);
            w->setViewSize(ev.width, ev.height);
            // Because the ts layer calls the getviewsize interface in response to resize.
            // So we need to set the view size when sending the message.
            events::Resize::broadcast(ev.width, ev.height, ev.windowId);
            isHandled = true;
            break;
        }
        case WindowEvent::Type::HIDDEN:
        case WindowEvent::Type::MINIMIZED:
            emit<EngineStatusChange>(ON_PAUSE);
#if CC_PLATFORM == CC_PLATFORM_WINDOWS
            events::WindowDestroy::broadcast(ev.windowId);
#endif
            events::EnterBackground::broadcast();

            isHandled = true;
            break;
        case WindowEvent::Type::CLOSE:
            emit<EngineStatusChange>(ON_CLOSE);
            events::Close::broadcast();
            // Increase the frame rate and get the program to exit as quickly as possible
            setPreferredFramesPerSecond(1000);
            isHandled = true;
            break;
        case WindowEvent::Type::QUIT:
            isHandled = true;
            break;
        case WindowEvent::Type::ENTER: {
            events::WindowEnter::broadcast();
            isHandled = true;
            break;
        }
        case WindowEvent::Type::LEAVE: {
            events::WindowLeave::broadcast();
            isHandled = true;
            break;
        }
        default:
            break;
    }

    return isHandled;
}

} // namespace cc
