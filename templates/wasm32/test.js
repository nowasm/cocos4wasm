"use strict";

(function () {
  const MAX_DEPTH = 3;
  const visited = new WeakSet();
  const summary = {
    namespaces: 0,
    classes: 0,
    constructors: 0,
    staticMethods: 0,
    instanceMethods: 0,
    failures: [],
  };

  const engineRoots = [
    "cc",
    "jsb",
    "gfx",
    "render",
    "pipeline",
    "geometry",
    "gi",
    "middleware",
    "spine",
    "dragonBones",
    "box2d",
  ];

  const skipMethodPattern = /^(destroy|release|finalize|cleanup|close|dispose|removeAll|remove|delete|erase|clear|reset|free|exit|shutdown|submit|present|flush|commit|tick|run|start|stop|pause|resume)$/i;
  const skipPropertyPattern = /^(prototype|constructor|arguments|caller|length|name)$/;

  function log(message) {
    console.log("[swig-test]", message);
  }

  function fail(label, error) {
    const detail = error && error.message ? error.message : String(error);
    summary.failures.push(`${label}: ${detail}`);
    console.error("[swig-test][FAIL]", label, detail);
  }

  function safeCall(label, fn) {
    try {
      return { ok: true, value: fn() };
    } catch (error) {
      fail(label, error);
      return { ok: false, value: undefined };
    }
  }

  function isObjectLike(value) {
    return value && (typeof value === "object" || typeof value === "function");
  }

  function isClassLike(fn) {
    if (typeof fn !== "function") {
      return false;
    }
    if (!fn.prototype) {
      return false;
    }
    const names = Object.getOwnPropertyNames(fn.prototype).filter((name) => name !== "constructor");
    return names.length > 0;
  }

  function shouldTryMethod(name, fn) {
    return typeof fn === "function" && fn.length === 0 && !skipMethodPattern.test(name);
  }

  function smokeConstructor(path, ctor) {
    summary.classes += 1;
    if (ctor.length !== 0) {
      return;
    }

    const result = safeCall(`${path} ctor`, () => Reflect.construct(ctor, []));
    if (!result.ok || !isObjectLike(result.value)) {
      return;
    }

    summary.constructors += 1;
    smokeInstance(path, result.value);
  }

  function smokeInstance(path, instance) {
    const proto = Object.getPrototypeOf(instance);
    if (!proto) {
      return;
    }
    for (const name of Object.getOwnPropertyNames(proto)) {
      if (skipPropertyPattern.test(name)) {
        continue;
      }
      const value = instance[name];
      if (!shouldTryMethod(name, value)) {
        continue;
      }
      const result = safeCall(`${path}.${name}()`, () => value.call(instance));
      if (result.ok) {
        summary.instanceMethods += 1;
      }
    }
  }

  function smokeStaticMethods(path, object) {
    for (const name of Object.getOwnPropertyNames(object)) {
      if (skipPropertyPattern.test(name)) {
        continue;
      }

      let value;
      try {
        value = object[name];
      } catch (error) {
        fail(`${path}.${name}`, error);
        continue;
      }

      if (shouldTryMethod(name, value)) {
        const result = safeCall(`${path}.${name}()`, () => value.call(object));
        if (result.ok) {
          summary.staticMethods += 1;
        }
      }
    }
  }

  function walkNamespace(path, value, depth) {
    if (!isObjectLike(value) || visited.has(value) || depth > MAX_DEPTH) {
      return;
    }
    visited.add(value);
    summary.namespaces += 1;

    smokeStaticMethods(path, value);

    for (const name of Object.getOwnPropertyNames(value)) {
      if (skipPropertyPattern.test(name)) {
        continue;
      }

      let child;
      try {
        child = value[name];
      } catch (error) {
        fail(`${path}.${name}`, error);
        continue;
      }

      const childPath = `${path}.${name}`;
      if (isClassLike(child)) {
        smokeConstructor(childPath, child);
      }

      if (isObjectLike(child)) {
        walkNamespace(childPath, child, depth + 1);
      }
    }
  }

  function smokeGlobals() {
    safeCall("TextEncoder", () => {
      const bytes = new TextEncoder().encode("cocos4wasm");
      if (!(bytes instanceof Uint8Array) || bytes.length === 0) {
        throw new Error("unexpected TextEncoder result");
      }
    });

    safeCall("TextDecoder", () => {
      const text = new TextDecoder().decode(new Uint8Array([99, 52]));
      if (text !== "c4") {
        throw new Error(`unexpected TextDecoder result: ${text}`);
      }
    });

    safeCall("performance.now", () => {
      const now = performance.now();
      if (typeof now !== "number") {
        throw new Error("performance.now did not return a number");
      }
    });

    if (typeof __getPlatform === "function") {
      safeCall("__getPlatform", __getPlatform);
    }
    if (typeof __getOS === "function") {
      safeCall("__getOS", __getOS);
    }
    if (typeof __getCurrentLanguage === "function") {
      safeCall("__getCurrentLanguage", __getCurrentLanguage);
    }
  }

  log("Starting swig export smoke test");
  smokeGlobals();

  for (const rootName of engineRoots) {
    if (!Object.prototype.hasOwnProperty.call(globalThis, rootName)) {
      continue;
    }
    log(`Walking namespace ${rootName}`);
    walkNamespace(rootName, globalThis[rootName], 0);
  }

  log(`Namespaces walked: ${summary.namespaces}`);
  log(`Classes discovered: ${summary.classes}`);
  log(`Zero-arg constructors passed: ${summary.constructors}`);
  log(`Zero-arg static methods passed: ${summary.staticMethods}`);
  log(`Zero-arg instance methods passed: ${summary.instanceMethods}`);

  if (summary.failures.length > 0) {
    log(`Failures: ${summary.failures.length}`);
    for (const item of summary.failures) {
      console.error("[swig-test][SUMMARY]", item);
    }
    const details = summary.failures.slice(0, 5).join(" | ");
    throw new Error(`swig smoke test failed with ${summary.failures.length} failures: ${details}`);
  }

  log("Swig export smoke test finished successfully");
})();
