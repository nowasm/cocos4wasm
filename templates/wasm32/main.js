"use strict";

/**
 * main.js — Entry point for the WASM runtime (standalone, no Cocos Creator).
 *
 * The C++ engine (BaseGame) has already initialised:
 *   - GFX Device (WebGL 2.0)
 *   - Root + ForwardPipeline
 *   - Builtin effects (shaders/materials)
 *
 * The wasm32 facade (globalThis.cc) attaches a camera to the scene's
 * native RenderScene when director.runSceneImmediate() is called.
 *
 * Edit this file to build your own application.
 */

(function () {
  if (!globalThis.cc) {
    throw new Error("cc facade is unavailable");
  }

  var cc   = globalThis.cc;
  var view = cc.view;
  var vs   = view.getVisibleSize();

  console.log("[main] visible size: " + vs.width + "x" + vs.height);

  // ---- Create the scene ----
  var scene = new cc.Scene("MainScene");

  // Canvas root node
  var canvasNode = new cc.Node("Canvas");
  scene.addChild(canvasNode);
  canvasNode.addComponent(cc.Canvas);

  var canvasTransform = canvasNode.addComponent(cc.UITransform);
  canvasTransform.setContentSize(vs.width, vs.height);

  // ---- Test button (centered) ----
  var buttonNode = new cc.Node("TestButton");
  canvasNode.addChild(buttonNode);

  var btnTransform = buttonNode.addComponent(cc.UITransform);
  btnTransform.setContentSize(240, 80);
  buttonNode.addComponent(cc.Button);

  if (cc.Widget) {
    var w = buttonNode.addComponent(cc.Widget);
    w.isAlignHorizontalCenter = true;
    w.isAlignVerticalCenter   = true;
    w.horizontalCenter = 0;
    w.verticalCenter   = 0;
  } else {
    buttonNode.setPosition(0, 0, 0);
  }

  buttonNode.on(cc.Node.EventType.TOUCH_END, function () {
    console.log("[main] Button clicked!");
  });

  // ---- Run the scene ----
  cc.director.runSceneImmediate(scene);

  // Sync the UI root so the native 2D batcher can render it.
  if (cc.__wasm32UI) {
    cc.__wasm32UI.syncRootNode(canvasNode);
  }

  console.log("[main] scene ready");
})();
