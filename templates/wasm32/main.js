"use strict";

/**
 * main.js — Entry point for the WASM runtime (standalone, no Cocos Creator).
 *
 * This script runs after the native engine has fully initialised the QuickJS
 * context and the wasm32 facade (globalThis.cc).  It creates a minimal scene
 * with a centred button to verify that rendering, input and the component
 * system are working.
 *
 * Edit this file to build your own application.
 */

(function () {
  if (!globalThis.cc) {
    throw new Error("cc facade is unavailable — engine did not initialise correctly");
  }

  const {
    director,
    view,
    Scene,
    Node,
    Canvas,
    UITransform,
    Widget,
    Button,
    Label,
    Color,
  } = globalThis.cc;

  // ---------------------------------------------------------------------------
  // 1. Create the root scene
  // ---------------------------------------------------------------------------
  const scene = new Scene("MainScene");

  // ---------------------------------------------------------------------------
  // 2. Canvas node — acts as the UI root and defines the design resolution
  // ---------------------------------------------------------------------------
  const canvasNode = new Node("Canvas");
  scene.addChild(canvasNode);
  canvasNode.addComponent(Canvas);

  const canvasTransform = canvasNode.addComponent(UITransform);
  const visibleSize = view.getVisibleSize();
  canvasTransform.setContentSize(visibleSize.width, visibleSize.height);

  // ---------------------------------------------------------------------------
  // 3. Title label
  // ---------------------------------------------------------------------------
  if (Label) {
    const titleNode = new Node("Title");
    canvasNode.addChild(titleNode);

    const titleTransform = titleNode.addComponent(UITransform);
    titleTransform.setContentSize(400, 60);

    const label = titleNode.addComponent(Label);
    label.string = "Cocos4WASM Runtime";
    label.fontSize = 32;
    label.horizontalAlign = Label.HorizontalAlign.CENTER;
    label.verticalAlign = Label.VerticalAlign.CENTER;

    if (Widget) {
      const w = titleNode.addComponent(Widget);
      w.isAlignHorizontalCenter = true;
      w.isAlignVerticalCenter = true;
      w.horizontalCenter = 0;
      w.verticalCenter = 100;
    } else {
      titleNode.setPosition(0, 100, 0);
    }
  }

  // ---------------------------------------------------------------------------
  // 4. Test button — centred in the viewport
  // ---------------------------------------------------------------------------
  const buttonNode = new Node("TestButton");
  canvasNode.addChild(buttonNode);

  const buttonTransform = buttonNode.addComponent(UITransform);
  buttonTransform.setContentSize(240, 80);
  buttonNode.addComponent(Button);

  if (Widget) {
    const widget = buttonNode.addComponent(Widget);
    widget.isAlignHorizontalCenter = true;
    widget.isAlignVerticalCenter = true;
    widget.horizontalCenter = 0;
    widget.verticalCenter = 0;
  } else {
    buttonNode.setPosition(0, 0, 0);
  }

  // Button click handler
  buttonNode.on(Node.EventType.TOUCH_END, function () {
    console.log("[main] Button clicked!");
  });

  // ---------------------------------------------------------------------------
  // 5. Run the scene
  // ---------------------------------------------------------------------------
  director.runSceneImmediate(scene);

  // Sync the UI root so the native 2D batcher can render it.
  if (globalThis.cc.__wasm32UI) {
    globalThis.cc.__wasm32UI.syncRootNode(canvasNode);
  }

  // Keep references on globalThis for debugging from the browser console.
  globalThis.__scene = scene;
  globalThis.__buttonNode = buttonNode;

  console.log("[main] Scene created — " + visibleSize.width + "x" + visibleSize.height);
})();
