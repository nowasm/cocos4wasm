/**
 * prepare-wasm-adapter
 *
 * Converts the jsb-adapter CommonJS modules into a single flat JS file
 * that does NOT use require() or browserify.  Each module is wrapped in
 * an IIFE and evaluated sequentially so that the WASM call depth stays
 * within the browser's ~10K frame limit.
 *
 * Usage:
 *   node src/index.js prepare-wasm-adapter --engine <engine-path> --out <output-dir>
 */
const fs = require('fs-extra');
const path = require('path');

const ADAPTER_SRC = 'platforms/native/builtin';
const JSB_ADAPTER_SRC = 'platforms/native/builtin/jsb-adapter';

function getModuleList(engineDir) {
    const a = path.join(engineDir, JSB_ADAPTER_SRC);
    const b = path.join(engineDir, ADAPTER_SRC);

    // Topologically sorted by dependency graph.
    return [
        // --- polyfills (no adapter deps) ---
        { file: path.join(b, 'wasm.js'), comment: 'wasm polyfill' },
        { file: path.join(b, 'base64', 'base64.min.js'), comment: 'base64' },
        { file: path.join(b, 'Blob.js'), comment: 'Blob + URL' },
        { file: path.join(b, 'xmldom', 'entities.js'), comment: 'xmldom/entities' },
        { file: path.join(b, 'xmldom', 'dom.js'), comment: 'xmldom/dom' },
        { file: path.join(b, 'xmldom', 'sax.js'), comment: 'xmldom/sax' },
        { file: path.join(b, 'xmldom', 'dom-parser.js'), comment: 'xmldom/dom-parser' },

        // --- Level 0: no adapter deps ---
        { file: path.join(a, 'EventTarget.js'), comment: 'EventTarget' },
        { file: path.join(a, 'Event.js'), comment: 'Event' },
        { file: path.join(a, 'DOMRect.js'), comment: 'DOMRect' },
        { file: path.join(a, 'ImageData.js'), comment: 'ImageData' },
        { file: path.join(a, 'MediaError.js'), comment: 'MediaError' },
        { file: path.join(a, 'FontFace.js'), comment: 'FontFace' },
        { file: path.join(a, 'fetch.js'), comment: 'fetch' },

        // --- Level 1: EventTarget/Event ---
        { file: path.join(a, 'Node.js'), comment: 'Node (DOM)' },
        { file: path.join(a, 'TouchEvent.js'), comment: 'TouchEvent' },
        { file: path.join(a, 'MouseEvent.js'), comment: 'MouseEvent' },
        { file: path.join(a, 'KeyboardEvent.js'), comment: 'KeyboardEvent' },
        { file: path.join(a, 'DeviceMotionEvent.js'), comment: 'DeviceMotionEvent' },
        { file: path.join(a, 'FontFaceSet.js'), comment: 'FontFaceSet' },
        { file: path.join(a, 'FileReader.js'), comment: 'FileReader' },

        // --- Level 2: Node ---
        { file: path.join(a, 'Element.js'), comment: 'Element' },

        // --- Level 3: Element ---
        { file: path.join(a, 'util', 'index.js'), comment: 'util' },
        { file: path.join(a, 'HTMLElement.js'), comment: 'HTMLElement' },

        // --- Level 4: HTMLElement ---
        { file: path.join(a, 'CanvasRenderingContext2D.js'), comment: 'CanvasRenderingContext2D' },
        { file: path.join(a, 'HTMLImageElement.js'), comment: 'HTMLImageElement' },
        { file: path.join(a, 'HTMLMediaElement.js'), comment: 'HTMLMediaElement' },
        { file: path.join(a, 'HTMLScriptElement.js'), comment: 'HTMLScriptElement' },
        { file: path.join(a, 'HTMLCanvasElement.js'), comment: 'HTMLCanvasElement' },

        // --- Level 5: HTMLMediaElement ---
        { file: path.join(a, 'HTMLVideoElement.js'), comment: 'HTMLVideoElement' },
        { file: path.join(a, 'HTMLAudioElement.js'), comment: 'HTMLAudioElement' },

        // --- Level 6 ---
        { file: path.join(a, 'Image.js'), comment: 'Image' },
        { file: path.join(a, 'Audio.js'), comment: 'Audio' },

        // --- Level 7: location, navigator, document (used by window.js) ---
        { file: path.join(a, 'location.js'), comment: 'location' },
        { file: path.join(a, 'navigator.js'), comment: 'navigator' },
        { file: path.join(a, 'document.js'), comment: 'document' },

        // --- builtin helpers (depend on EventTarget/Event) ---
        { file: path.join(b, 'jsb_prepare.js'), comment: 'jsb_prepare' },
        { file: path.join(b, 'jsb_audioengine.js'), comment: 'jsb_audioengine' },
        { file: path.join(b, 'jsb_input.js'), comment: 'jsb_input' },

        // --- Promise polyfill (after setTimeout is set up) ---
        { file: path.join(b, 'promise.min.js'), comment: 'Promise polyfill' },
    ];
}

/**
 * Transform a CommonJS module source into an IIFE that uses the shared
 * `__modules` registry instead of require().
 */
function wrapModule(source, moduleName, comment) {
    let transformed = source;

    // Replace require('./xxx') and require('../xxx') patterns
    transformed = transformed.replace(
        /require\s*\(\s*['"](?:\.\/|\.\.\/)*([^'"]+)['"]\s*\)/g,
        (match, modPath) => {
            const name = modPath.replace(/\.js$/, '').replace(/.*\//, '');
            return `__modules['${name}']`;
        }
    );

    // Strip ES module syntax, track exported names
    const exportedNames = [];
    transformed = transformed.replace(/^export\s+function\s+(\w+)/gm, (m, name) => {
        exportedNames.push(name); return `function ${name}`;
    });
    transformed = transformed.replace(/^export\s+class\s+(\w+)/gm, (m, name) => {
        exportedNames.push(name); return `class ${name}`;
    });
    transformed = transformed.replace(/^export\s+var\s+(\w+)/gm, (m, name) => {
        exportedNames.push(name); return `var ${name}`;
    });
    transformed = transformed.replace(/^export\s+let\s+(\w+)/gm, (m, name) => {
        exportedNames.push(name); return `let ${name}`;
    });
    transformed = transformed.replace(/^export\s+const\s+(\w+)/gm, (m, name) => {
        exportedNames.push(name); return `const ${name}`;
    });
    transformed = transformed.replace(/^export\s+\{[^}]*\}\s*;?\s*$/gm, '');
    transformed = transformed.replace(/^export\s+default\s+/gm, `module.exports = `);

    if (exportedNames.length > 0) {
        const assigns = exportedNames.map(n => `module.exports['${n}'] = ${n};`).join('\n');
        transformed += `\n${assigns}\n`;
    }

    // Wrap in IIFE with CommonJS-like locals
    return `// --- ${comment || moduleName} ---\n(function() {\nvar exports = __modules['${moduleName}'] = __modules['${moduleName}'] || {};\nvar module = { exports: exports };\n${transformed}\n__modules['${moduleName}'] = module.exports;\n})();\n`;
}

function transformRequires(source) {
    return source.replace(
        /require\s*\(\s*['"](?:\.\/|\.\.\/)*([^'"]+)['"]\s*\)/g,
        (match, modPath) => {
            const name = modPath.replace(/\.js$/, '').replace(/.*\//, '');
            return `__modules['${name}']`;
        }
    ).replace(/module\.exports\s*=/g, '/* module.exports = */');
}

async function prepareWasmAdapter(engineDir, outputDir) {
    console.log(`Engine: ${engineDir}`);
    console.log(`Output: ${outputDir}`);

    const modules = getModuleList(engineDir);

    let output = `// Auto-generated by prepare-wasm-adapter for WASM32 platform.
// Flat concatenation of jsb-adapter modules (no browserify wrapper).
"use strict";
var __modules = {};
globalThis.__EDITOR__ = false;
if (typeof self === 'undefined') { globalThis.self = globalThis; }

// jsbWindow (shared state)
var jsbWindow = globalThis.jsb.window = globalThis.jsb.window || {};
__modules['jsbWindow'] = jsbWindow;
__modules['../jsbWindow'] = jsbWindow;

`;

    for (const mod of modules) {
        if (!fs.existsSync(mod.file)) {
            console.warn(`  SKIP (not found): ${mod.file}`);
            output += `// SKIPPED: ${mod.comment} (file not found)\n`;
            continue;
        }

        const source = fs.readFileSync(mod.file, 'utf-8');
        const baseName = path.basename(mod.file, '.js');
        const moduleName = baseName === 'index' ? 'util' : baseName;

        console.log(`  + ${mod.comment} (${baseName})`);
        output += wrapModule(source, moduleName, mod.comment);
        output += '\n';
    }

    // Append main entry (index.js) and window.js injection
    const indexSrc = fs.readFileSync(path.join(engineDir, ADAPTER_SRC, 'index.js'), 'utf-8');
    output += `// --- Main entry (index.js) ---\n(function() {\n${transformRequires(indexSrc)}\n})();\n`;

    const windowSrc = fs.readFileSync(path.join(engineDir, JSB_ADAPTER_SRC, 'window.js'), 'utf-8');
    output += `\n// --- jsb-adapter/window.js (injection) ---\n(function() {\n${transformRequires(windowSrc)}\n})();\n`;

    await fs.ensureDir(outputDir);
    const outFile = path.join(outputDir, 'web-adapter.js');
    fs.writeFileSync(outFile, output, 'utf-8');
    console.log(`\nWritten: ${outFile} (${(output.length / 1024).toFixed(1)} KB)`);

    // Unbundle engine-adapter.js (also a browserify bundle that overflows WASM call stack)
    const ENGINE_ADAPTER_SRC = 'platforms/native/engine';
    const eaDir = path.join(engineDir, ENGINE_ADAPTER_SRC);
    const eaFiles = [
        'jsb-fs-utils.js',       // must be first (exports getUserDataPath etc.)
        'jsb-cache-manager.js',   // depends on jsb-fs-utils
        'jsb-assets-manager.js',
        'jsb-game.js',
        'jsb-gfx.js',
        'jsb-loader.js',
        'jsb-videoplayer.js',
        'jsb-webview.js',
        'jsb-editbox.js',
        'jsb-editor-support.js',
        'jsb-spine-skeleton.js',
        'jsb-dragonbones.js',
        'jsb-physics.js',
    ];

    let eaOutput = `// Auto-generated engine-adapter for WASM32 (unbundled).\n"use strict";\n`;
    for (const f of eaFiles) {
        const fPath = path.join(eaDir, f);
        if (!fs.existsSync(fPath)) {
            console.log(`  EA skip: ${f}`);
            eaOutput += `// SKIPPED: ${f}\n`;
            continue;
        }
        const src = fs.readFileSync(fPath, 'utf-8');
        const name = f.replace(/\.js$/, '');
        console.log(`  EA+ ${name}`);
        // Wrap each file in an IIFE; replace require('./xxx') with inline
        // (engine-adapter modules don't export, they modify globals)
        let transformed = src.replace(
            /require\s*\(\s*['"](?:\.\/|\.\.\/)*([^'"]+)['"]\s*\)/g,
            (match, modPath) => {
                // These modules reference each other or the web-adapter's modules
                const mname = modPath.replace(/\.js$/, '').replace(/.*\//, '');
                return `(__modules['${mname}'] || {})`;
            }
        );
        // Provide module/exports locals and capture exports in __modules
        eaOutput += `// --- ${name} ---\n(function() {\nvar exports = __modules['${name}'] = __modules['${name}'] || {};\nvar module = { exports: exports };\ntry {\n${transformed}\n__modules['${name}'] = module.exports;\n} catch(e) { console.warn('engine-adapter ${name}:', e.message); }\n})();\n\n`;
    }

    // Append the entry (index.js) - processes require calls
    const eaIndexSrc = fs.readFileSync(path.join(eaDir, 'index.js'), 'utf-8');
    let eaIndexTransformed = eaIndexSrc.replace(
        /require\s*\(\s*['"](?:\.\/|\.\.\/)*([^'"]+)['"]\s*\)/g,
        (match, modPath) => `/* ${match} - already loaded */`
    );
    eaOutput += `// --- engine-adapter index ---\n(function() {\n${eaIndexTransformed}\n})();\n`;

    const eaDest = path.join(outputDir, 'engine-adapter.js');
    fs.writeFileSync(eaDest, eaOutput, 'utf-8');
    console.log(`Written: ${eaDest} (${(eaOutput.length / 1024).toFixed(1)} KB)`);

    return outFile;
}

module.exports = prepareWasmAdapter;
