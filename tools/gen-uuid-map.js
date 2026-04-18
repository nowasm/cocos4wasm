#!/usr/bin/env node
// gen-uuid-map.js — walks an asset directory tree, reads every *.meta, and
// emits a single uuid-map.json that maps uuid → relative asset path.
//
// Usage:
//   node tools/gen-uuid-map.js <assets-dir> <output-path>
//
// The runtime AssetManager loads the output at startup; all subsequent
// __uuid__ references in scene/prefab JSONs resolve via this map.
//
// Meta format (minimal, matches Cocos Creator 3.x): the .meta file is
// JSON with a top-level "uuid" field. Sub-metas (sub-assets within a
// single source file) are not supported here — the MVP treats one .meta
// as one UUID → one file.

'use strict';

const fs = require('fs');
const path = require('path');

function main() {
    const args = process.argv.slice(2);
    if (args.length !== 2) {
        console.error('usage: node gen-uuid-map.js <assets-dir> <output-path>');
        process.exit(1);
    }

    const assetsDir = path.resolve(args[0]);
    const outPath = path.resolve(args[1]);

    if (!fs.existsSync(assetsDir)) {
        console.error(`assets dir does not exist: ${assetsDir}`);
        process.exit(1);
    }

    const map = {};
    let metaCount = 0;
    let entryCount = 0;

    walk(assetsDir, (file) => {
        if (!file.endsWith('.meta')) return;
        ++metaCount;

        const raw = fs.readFileSync(file, 'utf8');
        let meta;
        try {
            meta = JSON.parse(raw);
        } catch (e) {
            console.warn(`  skip ${path.relative(assetsDir, file)} — invalid JSON: ${e.message}`);
            return;
        }

        const uuid = meta && meta.uuid;
        if (typeof uuid !== 'string' || uuid.length === 0) return;

        // Asset path = meta path with ".meta" stripped, relative to assets root.
        const assetFile = file.replace(/\.meta$/, '');
        if (!fs.existsSync(assetFile)) {
            console.warn(`  skip ${uuid} — asset file missing: ${path.relative(assetsDir, assetFile)}`);
            return;
        }

        const rel = path.relative(assetsDir, assetFile).replace(/\\/g, '/');
        if (map[uuid] && map[uuid] !== rel) {
            console.warn(`  duplicate uuid ${uuid}: '${map[uuid]}' vs '${rel}' — keeping first`);
            return;
        }
        map[uuid] = rel;
        ++entryCount;
    });

    fs.mkdirSync(path.dirname(outPath), { recursive: true });
    fs.writeFileSync(outPath, JSON.stringify(map, null, 2) + '\n');

    console.log(`scanned ${metaCount} .meta files under ${assetsDir}`);
    console.log(`emitted ${entryCount} uuid entries to ${outPath}`);
}

function walk(dir, fileCb) {
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    for (const e of entries) {
        const full = path.join(dir, e.name);
        if (e.isDirectory()) {
            walk(full, fileCb);
        } else if (e.isFile()) {
            fileCb(full);
        }
    }
}

main();
