#include "jsb_wasm32_facade.h"

#include "bindings/jswrapper/SeApi.h"

namespace {

constexpr char WASM32_CC_FACADE_SCRIPT[] = R"JS(
(function (global) {
  if (global.cc) {
    return;
  }

  const jsb = global.jsb || {};
  const n2d = global.n2d || {};
  const gfx = global.gfx || {};
  const cc = {};
  const FORMAT_RG32F = 149;
  const FORMAT_RGB32F = 161;
  const FORMAT_RGBA32F = 174;
  const BUTTON_ACCESSOR_ID = 1;
  const BUTTON_BUFFER_ID = 0;
  const DRAW_INFO_TYPE_COMP = 0;
  const RENDER_ENTITY_TYPE_DYNAMIC = 1;
  const DRAW_STRIDE = 9;
  const ENTITY_BOOL_ENABLED = 0;
  const ATTR_BOOL_IS_MESH_BUFFER = 0;
  const ATTR_BOOL_IS_WORLD_POS = 1;
  const ATTR_U8_DRAW_INFO_TYPE = 0;
  const ATTR_U8_VERT_DIRTY = 1;
  const ATTR_U8_BOOLS = 2;
  const ATTR_U8_STRIDE = 3;
  const ATTR_U16_BUFFER_ID = 0;
  const ATTR_U16_ACCESSOR_ID = 1;
  const ATTR_U32_VERTEX_OFFSET = 0;
  const ATTR_U32_INDEX_OFFSET = 1;
  const ATTR_U32_VB_COUNT = 2;
  const ATTR_U32_IB_COUNT = 3;
  const ATTR_U32_DATA_HASH = 4;

  global.cc = cc;
  cc.__wasm32Facade = true;
  cc.Scene = jsb.Scene;
  cc.Node = jsb.Node;

  cc.Color = function Color(r, g, b, a) {
    this.r = r == null ? 255 : r;
    this.g = g == null ? 255 : g;
    this.b = b == null ? 255 : b;
    this.a = a == null ? 255 : a;
  };
  cc.Color.WHITE = new cc.Color(255, 255, 255, 255);

  cc.view = {
    getVisibleSize: function () {
      const win = jsb.window || {};
      return {
        width: win.innerWidth || 0,
        height: win.innerHeight || 0,
      };
    },
  };

  function ensureNodeState(node) {
    if (!node.__ccFacadeState) {
      node.__ccFacadeState = {
        components: [],
        listeners: Object.create(null),
      };
    }
    return node.__ccFacadeState;
  }

  function setBit(byteArray, bitIndex, enabled) {
    if (!byteArray || byteArray.length === 0) {
      return;
    }
    if (enabled) {
      byteArray[0] |= (1 << bitIndex);
    } else {
      byteArray[0] &= ~(1 << bitIndex);
    }
  }

  function getRoot() {
    // jsb.__rt is the JS-wrapped Root object set by BaseGame (C++ standalone mode).
    // jsb.Root.getInstance() may return null if the C++ Root has no JS wrapper.
    if (jsb.__rt) return jsb.__rt;
    return jsb.Root && typeof jsb.Root.getInstance === "function" ? jsb.Root.getInstance() : null;
  }

  function getBatcher2D() {
    const root = getRoot();
    return root && typeof root.getBatcher2D === "function" ? root.getBatcher2D() : null;
  }

  // Cache for builtin assets created from JS (Material objects need JS wrappers
  // for nativevalue_to_se to work — C++ ccnew Material has no JS counterpart).
  var _builtinAssetCache = Object.create(null);

  function getBuiltinAsset(name) {
    if (_builtinAssetCache[name]) return _builtinAssetCache[name];
    var mgr = jsb.BuiltinResMgr && typeof jsb.BuiltinResMgr.getInstance === "function" ? jsb.BuiltinResMgr.getInstance() : null;
    if (mgr && typeof mgr.getAsset === "function") {
      var asset = mgr.getAsset(name);
      if (asset) return asset;
    }
    return null;
  }

  function ensureBuiltinMaterials() {
    if (_builtinAssetCache["ui-base-material"]) return;
    if (!jsb.Material || !jsb.IMaterialInfo) return;
    var mgr = jsb.BuiltinResMgr && typeof jsb.BuiltinResMgr.getInstance === "function" ? jsb.BuiltinResMgr.getInstance() : null;

    var names = ["ui-base-material", "ui-sprite-material"];
    for (var i = 0; i < names.length; i++) {
      var mat = new jsb.Material();
      var info = new jsb.IMaterialInfo();
      info.effectName = "for2d/builtin-sprite";
      mat.initialize(info);
      _builtinAssetCache[names[i]] = mat;
      if (mgr && typeof mgr.addAsset === "function") {
        mgr.addAsset(names[i], mat);
      }
      console.log("[facade] created builtin material: " + names[i]);
    }
  }

  function createAttribute(name, format) {
    if (gfx.Attribute) {
      const attr = new gfx.Attribute();
      attr.name = name;
      attr.format = format;
      attr.isNormalized = false;
      attr.stream = 0;
      attr.isInstanced = false;
      attr.location = 0;
      return attr;
    }
    return {
      name: name,
      format: format,
      isNormalized: false,
      stream: 0,
      isInstanced: false,
      location: 0,
    };
  }

  // Reserve headroom for multiple UI draws sharing the same accessor/buffer in one frame,
  // and for any frame where fillIndexBuffers runs but batch emission is skipped (offsets must reset in native).
  const SHARED_UI_MAX_QUADS = 256;

  function createButtonMeshBuffer() {
    if (!n2d.UIMeshBuffer) {
      throw new Error("n2d.UIMeshBuffer is unavailable");
    }
    const meshBuffer = new n2d.UIMeshBuffer();
    const vData = new Float32Array(SHARED_UI_MAX_QUADS * DRAW_STRIDE);
    const iData = new Uint16Array(SHARED_UI_MAX_QUADS * 6);
    const layout = new Uint32Array(4);
    meshBuffer.vData = vData;
    meshBuffer.iData = iData;
    meshBuffer.initialize([
      createAttribute("a_position", FORMAT_RGB32F),
      createAttribute("a_texCoord", FORMAT_RG32F),
      createAttribute("a_color", FORMAT_RGBA32F),
    ]);
    // byteOffset / vertexOffset reflect the quad we upload for the button (rest of the buffer is unused).
    layout[0] = 4 * DRAW_STRIDE * 4;
    layout[1] = 4;
    layout[2] = 0;
    layout[3] = 0;
    meshBuffer.syncSharedBufferToNative(layout);
    if (typeof meshBuffer.setSharedIndexCapacity === "function") {
      meshBuffer.setSharedIndexCapacity(SHARED_UI_MAX_QUADS * 6);
    }
    return {
      native: meshBuffer,
      vData: vData,
      iData: iData,
      layout: layout,
    };
  }

  // UIMeshBuffer can be created before the render pipeline exists, but Batcher2d (and thus
  // syncMeshBuffersToNative / changeMeshBuffer) only exists after Root::setRenderPipeline().
  // Defer batcher registration until the pipeline is up; retry on rAF if main.js builds UI early.
  let sharedButtonMeshBuffer = null;
  let meshRegisteredWithBatcher = false;
  const pendingDrawInfosForBatcher = [];

  function getOrCreateSharedMeshOnly() {
    if (!sharedButtonMeshBuffer) {
      sharedButtonMeshBuffer = createButtonMeshBuffer();
    }
    return sharedButtonMeshBuffer;
  }

  function tryRegisterSharedMeshWithBatcher() {
    const batcher2D = getBatcher2D();
    if (!batcher2D || typeof batcher2D.syncMeshBuffersToNative !== "function") {
      return false;
    }
    if (!sharedButtonMeshBuffer) {
      return false;
    }
    if (!meshRegisteredWithBatcher) {
      batcher2D.syncMeshBuffersToNative(BUTTON_ACCESSOR_ID, [sharedButtonMeshBuffer.native]);
      meshRegisteredWithBatcher = true;
    }
    for (let i = 0; i < pendingDrawInfosForBatcher.length; ++i) {
      const di = pendingDrawInfosForBatcher[i];
      if (di && typeof di.changeMeshBuffer === "function") {
        di.changeMeshBuffer();
      }
    }
    pendingDrawInfosForBatcher.length = 0;
    return true;
  }

  let batcherLinkRetryHandle = null;
  let batcherLinkRetryFrames = 0;
  const BATCHER_LINK_MAX_FRAMES = 600;

  // Facade runs during jsb_register (before web-adapter); timers may be installed later on globalThis.
  function pickDeferSchedule() {
    const roots = [];
    try {
      if (typeof globalThis !== "undefined") {
        roots.push(globalThis);
      }
    } catch (e0) {}
    if (typeof global !== "undefined" && global !== null) {
      roots.push(global);
    }
    for (let ri = 0; ri < roots.length; ++ri) {
      const r = roots[ri];
      if (!r) {
        continue;
      }
      if (typeof r.requestAnimationFrame === "function") {
        const raf = r.requestAnimationFrame;
        return function (cb) {
          return raf.call(r, cb);
        };
      }
      if (typeof r.setTimeout === "function") {
        const st = r.setTimeout;
        return function (cb) {
          return st.call(r, cb, 16);
        };
      }
    }
    return null;
  }

  // Root only runs acquire / Batcher2d::uploadBuffers / pipeline render when at least one
  // enabled camera is attached to a RenderWindow (see Root::frameMoveProcess). Facade scenes
  // that never create a camera will stay black.
  const CAMERA_PROJECTION_ORTHO = 0;
  const CAMERA_USAGE_GAME = 100;
  const CLEAR_FLAG_COLOR_DEPTH = 3;

  function getSceneRenderScene(scene) {
    if (!scene) {
      return null;
    }
    if (typeof scene.getRenderScene === "function") {
      return scene.getRenderScene();
    }
    if (scene.renderScene) {
      return scene.renderScene;
    }
    return null;
  }

  function getJsRootFromScene(scene) {
    const rs = getSceneRenderScene(scene);
    if (rs && rs.root != null) {
      return rs.root;
    }
    return getRoot();
  }

  function pickMainRenderWindow(root) {
    if (!root) {
      return null;
    }
    let w = root.mainWindow;
    if (w) {
      return w;
    }
    try {
      if (typeof root.getMainWindow === "function") {
        w = root.getMainWindow();
        if (w) {
          return w;
        }
      }
    } catch (eGw) {}
    const wins = root.windows;
    if (wins && wins.length > 0) {
      return wins[0];
    }
    if (root.curWindow) {
      return root.curWindow;
    }
    return null;
  }

  let pendingWasm32CameraScene = null;
  let wasm32CameraRetryHandle = null;
  let wasm32CameraRetryFrames = 0;
  const WASM32_CAMERA_RETRY_MAX = 600;

  function scheduleWasm32CameraRetry(step) {
    if (wasm32CameraRetryHandle !== null) {
      return;
    }
    const deferSchedule = pickDeferSchedule();
    if (deferSchedule === null) {
      return;
    }
    const loop = function () {
      wasm32CameraRetryHandle = null;
      if (!pendingWasm32CameraScene) {
        wasm32CameraRetryFrames = 0;
        return;
      }
      if (step()) {
        pendingWasm32CameraScene = null;
        wasm32CameraRetryFrames = 0;
        return;
      }
      wasm32CameraRetryFrames += 1;
      if (wasm32CameraRetryFrames > WASM32_CAMERA_RETRY_MAX) {
        console.warn(
          "[wasm32-cc-facade] Main RenderWindow not available; wasm32 main camera was not created."
        );
        pendingWasm32CameraScene = null;
        wasm32CameraRetryFrames = 0;
        return;
      }
      const nextSchedule = pickDeferSchedule();
      if (nextSchedule === null) {
        return;
      }
      wasm32CameraRetryHandle = nextSchedule(loop);
    };
    wasm32CameraRetryHandle = deferSchedule(loop);
  }

  function createWasm32MainCameraNow(scene, root, mainWindow) {
    const renderScene = getSceneRenderScene(scene);
    if (!renderScene) {
      console.warn("[wasm32-cc-facade] No RenderScene on scene; camera not created.");
      return false;
    }
    const cams = renderScene.cameras;
    if (cams && cams.length > 0) {
      return true;
    }
    const Cam = jsb.Camera;
    const N = jsb.Node;
    if (!Cam || !N || typeof root.createCamera !== "function") {
      console.warn("[wasm32-cc-facade] jsb.Camera, jsb.Node or Root.createCamera unavailable.");
      return false;
    }
    const camNode = new N("Main Camera");
    scene.addChild(camNode);
    camNode.setPosition(0, 0, 1000);

    const camera = root.createCamera();
    const ok =
      typeof camera.initialize === "function" &&
      camera.initialize({
        name: "Main Camera",
        node: camNode,
        projection: CAMERA_PROJECTION_ORTHO,
        window: mainWindow,
        priority: 0,
        usage: CAMERA_USAGE_GAME,
      });
    if (!ok) {
      console.warn("[wasm32-cc-facade] Camera.initialize failed.");
      return false;
    }
    const vs = cc.view.getVisibleSize();
    const h = vs && vs.height > 0 ? vs.height : 640;
    camera.orthoHeight = Math.max(h * 0.5, 1);
    camera.nearClip = 0.1;
    camera.farClip = 2000;
    camera.visibility = 0xffffffff;
    camera.clearFlag = CLEAR_FLAG_COLOR_DEPTH;
    if (gfx && gfx.Color) {
      camera.clearColor = new gfx.Color(0.2, 0.2, 0.2, 1.0);
    }
    renderScene.addCamera(camera);
    return true;
  }

  function ensureWasm32MainCamera(scene) {
    pendingWasm32CameraScene = scene;
    const tryAttach = function () {
      const s = pendingWasm32CameraScene;
      if (!s) {
        return true;
      }
      const root = getJsRootFromScene(s);
      const win = pickMainRenderWindow(root);
      if (!root) {
        return false;
      }
      if (!win) {
        return false;
      }
      try {
        return createWasm32MainCameraNow(s, root, win);
      } catch (e) {
        console.warn("[wasm32-cc-facade] ensureWasm32MainCamera: " + e);
        pendingWasm32CameraScene = null;
        return true;
      }
    };
    if (tryAttach()) {
      pendingWasm32CameraScene = null;
      wasm32CameraRetryFrames = 0;
      return;
    }
    scheduleWasm32CameraRetry(tryAttach);
  }

  function scheduleBatcherLinkRetry() {
    if (batcherLinkRetryHandle !== null) {
      return;
    }
    const deferSchedule = pickDeferSchedule();
    if (deferSchedule === null) {
      return;
    }
    const step = function () {
      batcherLinkRetryHandle = null;
      if (pendingDrawInfosForBatcher.length === 0) {
        batcherLinkRetryFrames = 0;
        return;
      }
      if (tryRegisterSharedMeshWithBatcher()) {
        batcherLinkRetryFrames = 0;
        return;
      }
      batcherLinkRetryFrames += 1;
      if (batcherLinkRetryFrames > BATCHER_LINK_MAX_FRAMES) {
        console.warn(
          "[wasm32-cc-facade] Batcher2D did not become available in time; wasm32 UI may not render."
        );
        pendingDrawInfosForBatcher.length = 0;
        batcherLinkRetryFrames = 0;
        return;
      }
      const nextSchedule = pickDeferSchedule();
      if (nextSchedule === null) {
        return;
      }
      batcherLinkRetryHandle = nextSchedule(step);
    };
    batcherLinkRetryHandle = deferSchedule(step);
  }

  function createNativeRectRenderer(node, color, width, height) {
    ensureBuiltinMaterials();
    const mesh = getOrCreateSharedMeshOnly();
    const entity = new n2d.RenderEntity(RENDER_ENTITY_TYPE_DYNAMIC);
    const render2dBuffer = new Float32Array(4 * DRAW_STRIDE);
    entity.node = node;
    entity.renderTransform = node;

    const entityShared = entity.getEntitySharedBufferForJS();
    const entityU32 = new Uint32Array(entityShared, 0, 1);
    const entityU8 = new Uint8Array(entityShared, 4, 6);
    const entityBool = new Uint8Array(entityShared, 10, 1);
    entityU32[0] = 0;
    entityU8[0] = color.r;
    entityU8[1] = color.g;
    entityU8[2] = color.b;
    entityU8[3] = color.a;
    entityU8[4] = 0;
    entityU8[5] = 0;
    setBit(entityBool, ENTITY_BOOL_ENABLED, true);

    const drawInfo = new n2d.RenderDrawInfo();
    const localIb = new Uint16Array([0, 1, 2, 1, 3, 2]);
    if (!drawInfo) {
      throw new Error("failed to allocate dynamic RenderDrawInfo");
    }
    entity.addDynamicRenderDrawInfo(drawInfo);
    const drawShared = drawInfo.getAttrSharedBufferForJS();
    const drawU8 = new Uint8Array(drawShared, 0, 4);
    const drawU16 = new Uint16Array(drawShared, 4, 2);
    const drawU32 = new Uint32Array(drawShared, 8, 5);
    drawU8[ATTR_U8_DRAW_INFO_TYPE] = DRAW_INFO_TYPE_COMP;
    drawU8[ATTR_U8_VERT_DIRTY] = 0;
    drawU8[ATTR_U8_STRIDE] = DRAW_STRIDE;
    setBit(drawU8.subarray(ATTR_U8_BOOLS, ATTR_U8_BOOLS + 1), ATTR_BOOL_IS_MESH_BUFFER, false);
    setBit(drawU8.subarray(ATTR_U8_BOOLS, ATTR_U8_BOOLS + 1), ATTR_BOOL_IS_WORLD_POS, true);
    drawU16[ATTR_U16_BUFFER_ID] = BUTTON_BUFFER_ID;
    drawU16[ATTR_U16_ACCESSOR_ID] = BUTTON_ACCESSOR_ID;
    drawU32[ATTR_U32_VERTEX_OFFSET] = 0;
    drawU32[ATTR_U32_INDEX_OFFSET] = 0;
    drawU32[ATTR_U32_VB_COUNT] = 4;
    drawU32[ATTR_U32_IB_COUNT] = 6;
    drawU32[ATTR_U32_DATA_HASH] = 1;
    drawInfo.setRender2dBufferToNative(render2dBuffer);
    var mat = getBuiltinAsset("ui-base-material") || getBuiltinAsset("ui-sprite-material");
    console.log("[facade] createNativeRectRenderer: mat=" + !!mat + " type=" + typeof mat);
    drawInfo.material = mat;
    drawInfo.vbBuffer = mesh.vData;
    drawInfo.ibBuffer = localIb;
    drawInfo.vDataBuffer = mesh.vData;
    drawInfo.iDataBuffer = mesh.iData;
    if (tryRegisterSharedMeshWithBatcher()) {
      drawInfo.changeMeshBuffer();
    } else {
      pendingDrawInfosForBatcher.push(drawInfo);
      scheduleBatcherLinkRetry();
    }

    const vb = mesh.vData;
    const src = render2dBuffer;
    const halfWidth = width * 0.5;
    const halfHeight = height * 0.5;
    src.set([
      -halfWidth, -halfHeight, 0, 0, 0, 1, 1, 1, 1,
       halfWidth, -halfHeight, 0, 1, 0, 1, 1, 1, 1,
      -halfWidth,  halfHeight, 0, 0, 1, 1, 1, 1, 1,
       halfWidth,  halfHeight, 0, 1, 1, 1, 1, 1, 1,
    ]);
    vb.set(src);
    mesh.iData.set([0, 1, 2, 1, 3, 2]);

    return {
      entity: entity,
      drawInfo: drawInfo,
      localIb: localIb,
      render2dBuffer: render2dBuffer,
      width: width,
      height: height,
      color: color,
    };
  }

  function attachComponent(node, component) {
    component.node = node;
    ensureNodeState(node).components.push(component);
    if (typeof component.__onAttach === "function") {
      component.__onAttach();
    }
    return component;
  }

  if (cc.Node && cc.Node.prototype) {
    cc.Node.EventType = {
      TOUCH_END: "touch-end",
      CLICK: "click",
    };

    cc.Node.prototype.addComponent = function (Ctor) {
      return attachComponent(this, new Ctor());
    };

    cc.Node.prototype.getComponent = function (Ctor) {
      const components = ensureNodeState(this).components;
      for (let i = 0; i < components.length; ++i) {
        if (components[i] instanceof Ctor) {
          return components[i];
        }
      }
      return null;
    };

    cc.Node.prototype.on = function (type, handler) {
      const listeners = ensureNodeState(this).listeners;
      (listeners[type] || (listeners[type] = [])).push(handler);
    };

    cc.Node.prototype.emit = function (type, event) {
      const listeners = ensureNodeState(this).listeners[type];
      if (!listeners) {
        return;
      }
      for (let i = 0; i < listeners.length; ++i) {
        listeners[i](event);
      }
    };

    cc.Node.prototype.setPosition = function (x, y, z) {
      const px = x || 0;
      const py = y || 0;
      const pz = z || 0;
      this.__ccPosition = { x: px, y: py, z: pz };
      if (typeof this.setPositionForJS === "function") {
        this.setPositionForJS(px, py, pz);
      } else if (typeof this.setPositionInternal === "function") {
        this.setPositionInternal(px, py, pz);
      }
    };
  }

  cc.__wasm32UI = {
    syncRootNode: function (node) {
      tryRegisterSharedMeshWithBatcher();
      // Use the native C++ function registered by BaseGame — it can access
      // Batcher2D directly without JSB wrapper issues.
      if (typeof jsb.__syncBatcher2DRootNodes === "function") {
        jsb.__syncBatcher2DRootNodes(node);
      } else {
        // Fallback: try JSB path
        var batcher = getBatcher2D();
        if (batcher && typeof batcher.syncRootNodesToNative === "function") {
          batcher.syncRootNodesToNative([node]);
        }
      }
    },
    /** Call after render pipeline is ready if pending UI could not schedule rAF/setTimeout. */
    tryRegisterMeshWithBatcher: function () {
      return tryRegisterSharedMeshWithBatcher();
    },
  };

  cc.director = {
    runSceneImmediate: function (scene) {
      ensureBuiltinMaterials();
      global.__ccCurrentScene = scene;
      if (scene && typeof scene._load === "function") {
        scene._load();
      }
      if (scene && typeof scene._activate === "function") {
        scene._activate(true);
      }
      // Camera is auto-attached by C++ Engine::tick (ensureDefaultCameraOnScenes)
      // on the first frame after _activate creates the native RenderScene.
      tryRegisterSharedMeshWithBatcher();
    },
  };

  cc.Canvas = function Canvas() {};

  cc.UITransform = function UITransform() {
    this.width = 0;
    this.height = 0;
    this.anchorX = 0.5;
    this.anchorY = 0.5;
  };
  cc.UITransform.prototype.setContentSize = function (width, height) {
    this.width = width;
    this.height = height;
    if (this.node) {
      this.node.__ccSize = { width: width, height: height };
      const rectRenderer = this.node.__ccRectRenderer;
      if (rectRenderer) {
        rectRenderer.width = width;
        rectRenderer.height = height;
      }
    }
  };

  cc.Widget = function Widget() {
    this.isAlignHorizontalCenter = false;
    this.isAlignVerticalCenter = false;
    this.horizontalCenter = 0;
    this.verticalCenter = 0;
  };

  cc.Button = function Button() {
    this.transition = cc.Button.Transition.NONE;
    this.normalColor = new cc.Color(64, 132, 255, 255);
  };
  cc.Button.Transition = {
    NONE: 0,
  };
  cc.Button.prototype.__onAttach = function () {
    const ui = this.node.getComponent(cc.UITransform);
    const width = ui ? ui.width : 240;
    const height = ui ? ui.height : 80;
    this.node.__ccRectRenderer = createNativeRectRenderer(this.node, this.normalColor, width, height);
  };

  cc.Label = function Label() {
    this._string = "";
    this.fontSize = 20;
    this.lineHeight = 20;
    this.horizontalAlign = 0;
    this.verticalAlign = 0;
    this.color = new cc.Color();
  };
  cc.Label.HorizontalAlign = {
    LEFT: 0,
    CENTER: 1,
    RIGHT: 2,
  };
  cc.Label.VerticalAlign = {
    TOP: 0,
    CENTER: 1,
    BOTTOM: 2,
  };
  Object.defineProperty(cc.Label.prototype, "string", {
    get: function () {
      return this._string;
    },
    set: function (value) {
      this._string = String(value);
      if (this.node) {
        this.node.__ccLabelText = this._string;
      }
    },
  });
})(window);
)JS";

} // namespace

bool register_all_wasm32_facade(se::Object * /*global*/) {
#if CC_PLATFORM == CC_PLATFORM_EMSCRIPTEN
    auto *se = se::ScriptEngine::getInstance();
    return se->evalString(WASM32_CC_FACADE_SCRIPT, static_cast<uint32_t>(sizeof(WASM32_CC_FACADE_SCRIPT) - 1), nullptr, "wasm32-cc-facade.js");
#else
    return true;
#endif
}
