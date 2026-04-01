"use strict";

(function () {
  if (!globalThis.cc) {
    throw new Error("cc facade is unavailable");
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
  } = globalThis.cc;

  const scene = new Scene("RuntimeButtonScene");

  const canvasNode = new Node("Canvas");
  scene.addChild(canvasNode);
  canvasNode.addComponent(Canvas);

  const canvasTransform = canvasNode.addComponent(UITransform);
  const visibleSize = view.getVisibleSize();
  canvasTransform.setContentSize(visibleSize.width, visibleSize.height);

  const buttonNode = new Node("CenterButton");
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

  buttonNode.on(Node.EventType.TOUCH_END, function () {
    console.log("[test-scene] Button clicked");
  });

  director.runSceneImmediate(scene);
  if (globalThis.cc.__wasm32UI) {
    globalThis.cc.__wasm32UI.syncRootNode(canvasNode);
  }
  globalThis.__runtimeScene = scene;
  globalThis.__runtimeButtonNode = buttonNode;
  console.log("[test-scene] wasm32 WebGL button scene created");
})();
