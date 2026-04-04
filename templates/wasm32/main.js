"use strict";

/**
 * main.js — Entry point for the WASM runtime (standalone, no Cocos Creator).
 *
 * The C++ engine (BaseGame) has already initialised:
 *   - GFX Device (WebGL 2.0)
 *   - Root + ForwardPipeline
 *   - A default RenderScene with an ortho Camera
 *   - Builtin effects (shaders/materials)
 *
 * This script runs inside the QuickJS context.  The `cc` facade
 * (globalThis.cc) provides a lightweight scene-graph API.
 *
 * Edit this file to build your own application.
 */

(function () {
  console.log("[main.js] starting");

  if (!globalThis.cc) {
    console.error("[main.js] cc facade not available");
    return;
  }

  var view = globalThis.cc.view;
  var visibleSize = view ? view.getVisibleSize() : { width: 0, height: 0 };
  console.log("[main.js] visible size: " + visibleSize.width + "x" + visibleSize.height);

  // The engine is running — the dark-gray background you see is the
  // default camera created by the C++ runtime.
  //
  // TODO: scene-graph rendering requires additional JSB bindings for
  // models, meshes, and materials.  For now the runtime is verified
  // working when you see the background colour and this log message.

  console.log("[main.js] engine ready — runtime is operational");
})();
