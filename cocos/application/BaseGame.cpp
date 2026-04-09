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

#include "BaseGame.h"
#include <string>
#include "ApplicationManager.h"
#include "bindings/jswrapper/SeApi.h"
#include "platform/FileUtils.h"
#include "platform/interfaces/modules/ISystemWindowManager.h"
#include "renderer/pipeline/GlobalDescriptorSetManager.h"

#if CC_PLATFORM == CC_PLATFORM_EMSCRIPTEN && CC_WASM_STANDALONE_FACADE
    #include "2d/renderer/Batcher2d.h"
    #include "core/Root.h"
    #include "core/builtin/BuiltinResMgr.h"
    #include "core/builtin/BuiltinEffectLoader.h"
    #include "bindings/manual/jsb_classtype.h"
    #include "bindings/manual/jsb_conversions.h"
    #include "renderer/GFXDeviceManager.h"
    #include "renderer/pipeline/forward/ForwardPipeline.h"

// JSB function: jsb.__syncBatcher2DRootNodes(node)
// Called from the JS facade to sync root nodes for 2D rendering.
static bool js_syncBatcher2DRootNodes(se::State &s) {
    const auto &args = s.args();
    if (args.empty() || !args[0].isObject()) {
        CC_LOG_WARNING("__syncBatcher2DRootNodes: expected a Node argument");
        return false;
    }
    auto *node = static_cast<cc::Node *>(args[0].toObject()->getPrivateData());
    if (!node) {
        auto *privObj = args[0].toObject()->getPrivateObject();
        if (privObj) node = static_cast<cc::Node *>(privObj->getRaw());
    }
    if (!node) {
        CC_LOG_WARNING("__syncBatcher2DRootNodes: node has no native pointer");
        return false;
    }
    auto *root = cc::Root::getInstance();
    if (!root || !root->getBatcher2D()) {
        CC_LOG_WARNING("__syncBatcher2DRootNodes: Root or Batcher2D not available");
        return false;
    }
    ccstd::vector<cc::Node *> rootNodes;
    rootNodes.push_back(node);
    root->getBatcher2D()->syncRootNodesToNative(std::move(rootNodes));
    CC_LOG_INFO("__syncBatcher2DRootNodes: synced node '%s' to Batcher2D", node->getName().c_str());
    return true;
}
SE_BIND_FUNC(js_syncBatcher2DRootNodes)

#endif

#if CC_PLATFORM == CC_PLATFORM_ANDROID
    #include "platform/android/adpf_manager.h"
#endif
extern "C" void cc_load_all_plugins(); // NOLINT

namespace cc {

BaseGame::~BaseGame() { // NOLINT
#if (CC_PLATFORM == CC_PLATFORM_ANDROID) && CC_SUPPORT_ADPF
    ADPFManager::getInstance().destroy();
#endif
}

int BaseGame::init() {
    cc::pipeline::GlobalDSManager::setDescriptorSetLayout();

    cc_load_all_plugins();

#if (CC_PLATFORM == CC_PLATFORM_ANDROID) && CC_SUPPORT_ADPF
    ADPFManager::getInstance().initialize();
#endif

#if CC_PLATFORM == CC_PLATFORM_WINDOWS || CC_PLATFORM == CC_PLATFORM_LINUX || CC_PLATFORM == CC_PLATFORM_QNX || CC_PLATFORM == CC_PLATFORM_MACOS
    // override default value
    //_windowInfo.x      = _windowInfo.x == -1 ? 0 : _windowInfo.x;
    //_windowInfo.y      = _windowInfo.y == -1 ? 0 : _windowInfo.y;
    _windowInfo.width = _windowInfo.width == -1 ? 800 : _windowInfo.width;
    _windowInfo.height = _windowInfo.height == -1 ? 600 : _windowInfo.height;
    _windowInfo.flags = _windowInfo.flags == -1 ? cc::ISystemWindow::CC_WINDOW_SHOWN |
                                                      cc::ISystemWindow::CC_WINDOW_RESIZABLE |
                                                      cc::ISystemWindow::CC_WINDOW_INPUT_FOCUS
                                                : _windowInfo.flags;
    std::call_once(_windowCreateFlag, [&]() {
        ISystemWindowInfo info;
        info.title = _windowInfo.title;
    #if CC_PLATFORM == CC_PLATFORM_WINDOWS
        info.x = _windowInfo.x == -1 ? 50 : _windowInfo.x; // 50 meams move window a little for now
        info.y = _windowInfo.y == -1 ? 50 : _windowInfo.y; // same above
    #else
        info.x = _windowInfo.x == -1 ? 0 : _windowInfo.x;
        info.y = _windowInfo.y == -1 ? 0 : _windowInfo.y;
    #endif
        info.width = _windowInfo.width;
        info.height = _windowInfo.height;
        info.flags = _windowInfo.flags;

        ISystemWindowManager* windowMgr = CC_GET_PLATFORM_INTERFACE(ISystemWindowManager);
        windowMgr->createWindow(info);
    });

#endif

    if (_debuggerInfo.enabled) {
        setDebugIpAndPort(_debuggerInfo.address, _debuggerInfo.port, _debuggerInfo.pauseOnStart);
    }

    int ret = cc::CocosApplication::init();
    if (ret != 0) {
        return ret;
    }

    setXXTeaKey(_xxteaKey);
#if CC_PLATFORM == CC_PLATFORM_EMSCRIPTEN && CC_WASM_STANDALONE_FACADE
    // Standalone WASM mode: initialise Root + render pipeline from C++,
    // then run a minimal test main.js via the wasm32 facade.
    {
        BuiltinResMgr::getInstance()->initBuiltinRes();
        int effectCount = loadBuiltinEffectsFromJson("builtin-effects.json");
        CC_LOG_INFO("Loaded %d builtin effects for standalone WASM mode", effectCount);

        auto *device = gfx::Device::getInstance();
        CC_ASSERT(device);
        auto *root = ccnew Root(device);
        root->initialize(nullptr);
        auto *pipeline = ccnew pipeline::ForwardPipeline();
        pipeline->initialize({});
        root->setRenderPipeline(pipeline);

        auto *seEngine = se::ScriptEngine::getInstance();
        se::Value rootVal;
        nativevalue_to_se(root, rootVal);
        if (!rootVal.isObject()) {
            auto *rootCls = JSBClassType::findClass(root);
            if (rootCls) {
                auto *rootSeObj = se::Object::createObjectWithClass(rootCls);
                rootSeObj->setPrivateData(root);
                rootVal.setObject(rootSeObj);
            }
        }
        if (rootVal.isObject()) {
            se::Value jsbVal;
            seEngine->getGlobalObject()->getProperty("jsb", &jsbVal);
            if (jsbVal.isObject()) {
                jsbVal.toObject()->setProperty("__rt", rootVal);
                jsbVal.toObject()->defineFunction("__syncBatcher2DRootNodes", _SE(js_syncBatcher2DRootNodes));
            }
            CC_LOG_INFO("BaseGame: Root JS wrapper + __syncBatcher2DRootNodes registered");
        }
    }
#else
    // Full mode: set up the globals that jsb-adapter/web-adapter.js normally
    // provides, then run main.js which boots the full cc engine via SystemJS.
    //
    // We CANNOT evaluate web-adapter.js directly because it is a large
    // browserify bundle (~5800 lines, 40+ modules).  Each module factory
    // is invoked via Function.prototype.call(), and QuickJS's recursive
    // interpreter turns every .call() into ~4 C/WASM stack frames.
    // Chrome hard-limits WASM call depth to ~10K frames, so the bundle
    // overflows before it finishes initialising.
    //
    // Instead we replicate the essential setup with small eval snippets
    // that each unwind the C stack between them.
    {
        auto *se = se::ScriptEngine::getInstance();
        // Timer management (setTimeout, setInterval, requestAnimationFrame)
        // and the gameTick dispatcher – replicated from web-adapter.js module 5.
        se->evalString(R"JS(
(function() {
    var _requestAnimationFrameID = 0;
    var _requestAnimationFrameCallbacks = {};
    var _firstTick = true;
    window.requestAnimationFrame = function(cb) {
        var id = ++_requestAnimationFrameID;
        _requestAnimationFrameCallbacks[id] = cb;
        return id;
    };
    window.cancelAnimationFrame = function(id) {
        delete _requestAnimationFrameCallbacks[id];
    };
    var _timeoutIDIndex = 0;
    var _timeoutInfos = {};
    function fireTimeout(now) {
        for (var id in _timeoutInfos) {
            var info = _timeoutInfos[id];
            if (info && info.cb && now - info.start >= info.delay) {
                if (info.isRepeat) { info.start = now; }
                else { delete _timeoutInfos[id]; }
                if (typeof info.cb === 'function') info.cb.apply(info.target, info.args);
                else if (typeof info.cb === 'string') Function(info.cb)();
            }
        }
    }
    function createTimeoutInfo(args, isRepeat) {
        var cb = args[0]; if (!cb) return 0;
        var delay = args.length > 1 ? args[1] : 0;
        var a = args.length > 2 ? Array.prototype.slice.call(args, 2) : undefined;
        var id = ++_timeoutIDIndex;
        _timeoutInfos[id] = { cb:cb, id:id, start:performance.now(), delay:delay, isRepeat:isRepeat, target:this, args:a };
        return id;
    }
    window.setTimeout = function(cb) { return createTimeoutInfo(arguments, false); };
    window.clearTimeout = function(id) { delete _timeoutInfos[id]; };
    window.setInterval = function(cb) { return createTimeoutInfo(arguments, true); };
    window.clearInterval = window.clearTimeout;
    window.alert = console.error.bind(console);
    window.gameTick = function(nowMilliSeconds) {
        if (_firstTick) {
            _firstTick = false;
            if (window.onload) { var e = new Event('load'); window.onload(e); }
        }
        fireTimeout(nowMilliSeconds);
        for (var id in _requestAnimationFrameCallbacks) {
            var cb = _requestAnimationFrameCallbacks[id];
            if (cb) { delete _requestAnimationFrameCallbacks[id]; cb(nowMilliSeconds); }
        }
    };
})();
)JS", 0, nullptr, "web-adapter-timers.js");

        // jsb.window and jsb.device — used by cc engine for screen metrics
        se->evalString(R"JS(
(function() {
    if (typeof jsb === 'undefined') return;
    if (!jsb.window) jsb.window = {};
    var win = jsb.window;
    win.innerWidth = win.innerWidth || 800;
    win.innerHeight = win.innerHeight || 600;
    win.devicePixelRatio = 1;
    // cc.js uses `l = jsb.window` then accesses l.document, l.navigator, etc.
    // These must be on jsb.window, not just on globalThis.
    if (!win.document) {
        win.document = {
            createElement: function(tag) {
                var el = { style: {}, tagName: tag ? tag.toUpperCase() : 'DIV',
                    setAttribute: function(){}, getAttribute: function(){ return null; },
                    appendChild: function(){}, addEventListener: function(){},
                    getContext: function(){ return null; },
                    width: 0, height: 0 };
                if (tag === 'canvas') {
                    el.getContext = function(type) {
                        return { fillRect:function(){}, clearRect:function(){}, getImageData:function(x,y,w,h){ return {data:new Uint8Array(w*h*4)}; },
                            putImageData:function(){}, drawImage:function(){}, save:function(){}, restore:function(){},
                            fillText:function(){}, measureText:function(t){ return {width:t.length*8}; }, scale:function(){},
                            translate:function(){}, beginPath:function(){}, closePath:function(){}, arc:function(){}, fill:function(){},
                            stroke:function(){}, moveTo:function(){}, lineTo:function(){}, setTransform:function(){},
                            canvas:el, fillStyle:'', strokeStyle:'', lineWidth:1, font:'10px sans-serif', textAlign:'start', textBaseline:'alphabetic' };
                    };
                }
                return el;
            },
            createElementNS: function(ns, tag) { return win.document.createElement(tag); },
            querySelector: function() { return null; },
            querySelectorAll: function() { return []; },
            getElementById: function() { return null; },
            getElementsByTagName: function() { return []; },
            head: { appendChild: function(){}, removeChild: function(){} },
            body: { appendChild: function(){}, removeChild: function(){} },
            documentElement: { style: {} },
            addEventListener: function(){}
        };
    }
    // DOM element stubs — cc.js checks `instanceof HTMLElement` etc.
    if (!win.HTMLElement) { win.HTMLElement = function HTMLElement(){}; }
    if (!win.HTMLImageElement) { win.HTMLImageElement = function HTMLImageElement(){}; win.HTMLImageElement.prototype = Object.create(win.HTMLElement.prototype); }
    if (!win.HTMLCanvasElement) { win.HTMLCanvasElement = function HTMLCanvasElement(){}; win.HTMLCanvasElement.prototype = Object.create(win.HTMLElement.prototype); }
    if (!win.ImageBitmap) { win.ImageBitmap = function ImageBitmap(){}; }
    if (!win.Event) { win.Event = function Event(t){ this.type = t; }; }
    if (!win.navigator) {
        win.navigator = { userAgent: 'Mozilla/5.0 (wasm32; QuickJS) CocosCreator', language: 'en', platform: 'wasm' };
    }
    if (!win.location) {
        win.location = { href: '/data/', protocol: 'file:', hostname: '', pathname: '/data/', search: '', hash: '' };
    }
    // jsb.device — cc.js reads jsb.device.getDevicePixelRatio()
    if (!jsb.device && jsb.Device) { jsb.device = jsb.Device; }
    if (!jsb.device) {
        jsb.device = { getDevicePixelRatio: function() { return 1; } };
    }
    // screen object expected by some engine code
    if (typeof window.screen === 'undefined') {
        window.screen = { width: win.innerWidth, height: win.innerHeight, availWidth: win.innerWidth, availHeight: win.innerHeight };
    }
    // navigator
    if (typeof window.navigator === 'undefined') {
        window.navigator = { userAgent: 'Mozilla/5.0 (wasm32; QuickJS) CocosCocosCreator', language: 'en', platform: 'wasm' };
    }
    // document stub — SystemJS calls document.querySelector, createElement, etc.
    if (typeof window.document === 'undefined') {
        window.document = {
            createElement: function(tag) {
                var el = { style: {}, tagName: tag ? tag.toUpperCase() : 'DIV',
                    setAttribute: function(){}, getAttribute: function(){ return null; },
                    appendChild: function(){}, addEventListener: function(){} };
                return el;
            },
            createElementNS: function(ns, tag) { return window.document.createElement(tag); },
            querySelector: function() { return null; },
            querySelectorAll: function() { return []; },
            getElementById: function() { return null; },
            getElementsByTagName: function() { return []; },
            head: { appendChild: function(){}, removeChild: function(){} },
            body: { appendChild: function(){}, removeChild: function(){} },
            documentElement: { style: {} },
            addEventListener: function(){}
        };
    }
    // location stub — SystemJS reads location.href for base URL resolution
    if (typeof window.location === 'undefined') {
        window.location = { href: '/data/', protocol: 'file:', hostname: '', pathname: '/data/', search: '', hash: '' };
    }
})();
)JS", 0, nullptr, "web-adapter-jsb-window.js");

        // jsb.fileUtils setup
        se->evalString(R"JS(
if (typeof jsb !== 'undefined' && typeof jsb.FileUtils !== 'undefined') {
    jsb.fileUtils = jsb.FileUtils.getInstance();
    delete jsb.FileUtils;
}
)JS", 0, nullptr, "web-adapter-fileutils.js");

        // jsb.generateGetSet — creates JS property accessors from get/set methods
        se->evalString(R"JS(
if (typeof jsb !== 'undefined') {
    jsb.generateGetSet = function(moduleObj) {
        for (var classKey in moduleObj) {
            var classProto = moduleObj[classKey] && moduleObj[classKey].prototype;
            if (!classProto) continue;
            for (var getName in classProto) {
                if (getName.search(/^get/) === -1) continue;
                var propName = getName.replace(/^get/, '');
                var nameArr = propName.split('');
                var lowerFirst = nameArr[0].toLowerCase();
                var upperFirst = nameArr[0].toUpperCase();
                nameArr.splice(0, 1);
                var left = nameArr.join('');
                propName = lowerFirst + left;
                var setName = 'set' + upperFirst + left;
                if (classProto.hasOwnProperty(propName)) continue;
                var setFunc = classProto[setName];
                if (typeof setFunc === 'function') {
                    Object.defineProperty(classProto, propName, {
                        get: (function(g) { return function() { return this[g](); }; })(getName),
                        set: (function(s) { return function(v) { this[s](v); }; })(setName),
                        configurable: true
                    });
                } else {
                    Object.defineProperty(classProto, propName, {
                        get: (function(g) { return function() { return this[g](); }; })(getName),
                        configurable: true
                    });
                }
            }
        }
    };
}
)JS", 0, nullptr, "web-adapter-generateGetSet.js");

        // Minimal btoa/atob (used by some asset loaders)
        se->evalString(R"JS(
if (typeof window.btoa === 'undefined') {
    var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=';
    window.btoa = function(input) {
        var str = String(input), o = '';
        for (var b, c, h = 0, map = chars; str.charAt(h | 0) || (map = '=', h % 1);
             o += map.charAt(63 & b >> 8 - h % 1 * 8)) {
            c = str.charCodeAt(h += .75);
            b = b << 8 | c;
        }
        return o;
    };
    window.atob = function(input) {
        var str = String(input).replace(/[=]+$/, ''), o = '';
        for (var b, c, h = 0; c = str.charAt(h++);
             ~c && (b = h % 4 ? b * 64 + c : c, h++ % 4) ? o += String.fromCharCode(255 & b >> (-2 * h & 6)) : 0)
            c = chars.indexOf(c);
        return o;
    };
}
)JS", 0, nullptr, "web-adapter-base64.js");

        // XMLHttpRequest adapter
        se->evalString(R"JS(
if (typeof jsb !== 'undefined' && jsb.XMLHttpRequest) {
    window.XMLHttpRequest = jsb.XMLHttpRequest;
    if (window.XMLHttpRequest.prototype) {
        window.XMLHttpRequest.prototype.addEventListener = function(name, fn) { this['on' + name] = fn; };
        window.XMLHttpRequest.prototype.removeEventListener = function(name) { this['on' + name] = null; };
    }
}
if (typeof jsb !== 'undefined' && jsb.WebSocket) { window.WebSocket = jsb.WebSocket; }
)JS", 0, nullptr, "web-adapter-xhr.js");

        // Diagnostic: check what jsb has before running main.js
        se->evalString(R"JS(
(function() {
    var keys = [];
    for (var k in jsb) keys.push(k);
    console.log('[diag] jsb keys count: ' + keys.length);
    console.log('[diag] jsb.SimpleTexture = ' + typeof jsb.SimpleTexture);
    console.log('[diag] jsb.Node = ' + typeof jsb.Node);
    console.log('[diag] jsb.Scene = ' + typeof jsb.Scene);
    console.log('[diag] jsb.Material = ' + typeof jsb.Material);
    console.log('[diag] jsb.Root = ' + typeof jsb.Root);
    console.log('[diag] jsb.Device = ' + typeof jsb.Device);
})();
)JS", 0, nullptr, "diag-jsb.js");

        CC_LOG_INFO("WASM full mode: pre-stubs set up");
    }

    // Load the unbundled web-adapter.js — generated by:
    //   node tools/c4-cli/src/index.js prepare-wasm-adapter -e <engine> -o <out>
    // This is a flat concatenation of individual modules (no browserify
    // wrapper), so call depth stays within the browser's WASM limit.
    runScript("jsb-adapter/web-adapter.js");
    CC_LOG_INFO("WASM full mode: web-adapter.js loaded");
#endif
    runScript("main.js");

    // No init-tick loop needed — the normal Engine::tick() main loop
    // dispatches gameTick (timers/rAF), Scheduler::update (deferred
    // callbacks), and mainLoopUpdate (Promise microtasks). The cc.game
    // init chain completes asynchronously over the first few frames.

    return 0;
}
} // namespace cc
