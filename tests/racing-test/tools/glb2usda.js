// glb2usda.js — minimal GLB -> USDA converter for simple assets (Kenney-style):
// flat node hierarchy (translation/rotation/scale), triangle prims with
// POSITION/NORMAL/TEXCOORD_0, single pbrMetallicRoughness material with an
// embedded baseColorTexture. Emits <name>.usda + extracted texture png next to it.
//
// Usage: node glb2usda.js input.glb outdir/
'use strict';
const fs = require('fs');
const path = require('path');

const [, , inFile, outDir] = process.argv;
if (!inFile || !outDir) { console.error('usage: node glb2usda.js input.glb outdir/'); process.exit(1); }

const buf = fs.readFileSync(inFile);
if (buf.readUInt32LE(0) !== 0x46546c67) throw new Error('not a GLB');
const jsonLen = buf.readUInt32LE(12);
const gltf = JSON.parse(buf.slice(20, 20 + jsonLen).toString('utf8'));
// BIN chunk follows: [len][0x004E4942][data]
const binOfs = 20 + jsonLen;
const binLen = buf.readUInt32LE(binOfs);
if (buf.readUInt32LE(binOfs + 4) !== 0x004e4942) throw new Error('no BIN chunk');
const bin = buf.slice(binOfs + 8, binOfs + 8 + binLen);

const COMP = { 5120: Int8Array, 5121: Uint8Array, 5122: Int16Array, 5123: Uint16Array, 5125: Uint32Array, 5126: Float32Array };
const NCOMP = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4 };

function readAccessor(idx) {
  const a = gltf.accessors[idx];
  const bv = gltf.bufferViews[a.bufferView];
  const T = COMP[a.componentType];
  const n = NCOMP[a.type];
  const byteOfs = (bv.byteOffset || 0) + (a.byteOffset || 0);
  const stride = bv.byteStride || n * T.BYTES_PER_ELEMENT;
  const out = new T(a.count * n);
  for (let i = 0; i < a.count; i++) {
    for (let c = 0; c < n; c++) {
      const o = byteOfs + i * stride + c * T.BYTES_PER_ELEMENT;
      out[i * n + c] = new T(bin.buffer, bin.byteOffset + o, 1)[0];
    }
  }
  return out;
}

const f = (x) => {
  if (Number.isInteger(x)) return x.toString();
  return x.toFixed(6).replace(/0+$/, '0');
};
const sane = (s) => s.replace(/[^A-Za-z0-9_]/g, '_');

fs.mkdirSync(outDir, { recursive: true });
const baseName = sane(path.basename(inFile, path.extname(inFile)));

// ── extract first embedded image (Kenney: single colormap) ──────────────────
let texFile = null;
if (gltf.images && gltf.images.length) {
  const img = gltf.images[0];
  let data;
  let ext;
  if (img.uri) { // external file, relative to the glb
    data = fs.readFileSync(path.join(path.dirname(inFile), decodeURIComponent(img.uri)));
    ext = path.extname(img.uri).slice(1) || 'png';
  } else {
    const bv = gltf.bufferViews[img.bufferView];
    data = bin.slice(bv.byteOffset || 0, (bv.byteOffset || 0) + bv.byteLength);
    ext = (img.mimeType || 'image/png').includes('jpeg') ? 'jpg' : 'png';
  }
  texFile = `${baseName}_colormap.${ext}`;
  fs.writeFileSync(path.join(outDir, texFile), data);
}

// ── emit USDA ────────────────────────────────────────────────────────────────
let usda = `#usda 1.0
(
    defaultPrim = "${baseName}"
    metersPerUnit = 1
    upAxis = "Y"
)

def Xform "${baseName}"
{
`;

// material (single, shared)
if (texFile) {
  usda += `    def Scope "Materials"
    {
        def Material "colormap"
        {
            token outputs:surface.connect = </${baseName}/Materials/colormap/pbr.outputs:surface>

            def Shader "pbr"
            {
                uniform token info:id = "UsdPreviewSurface"
                color3f inputs:diffuseColor.connect = </${baseName}/Materials/colormap/tex.outputs:rgb>
                float inputs:metallic = 0
                float inputs:roughness = 0.9
                token outputs:surface
            }

            def Shader "stReader"
            {
                uniform token info:id = "UsdPrimvarReader_float2"
                token inputs:varname = "st"
                float2 outputs:result
            }

            def Shader "tex"
            {
                uniform token info:id = "UsdUVTexture"
                asset inputs:file = @${texFile}@
                float2 inputs:st.connect = </${baseName}/Materials/colormap/stReader.outputs:result>
                token inputs:wrapS = "repeat"
                token inputs:wrapT = "repeat"
                float3 outputs:rgb
            }
        }
    }

`;
}

const sceneNodes = gltf.scenes[gltf.scene || 0].nodes;
for (const ni of sceneNodes) {
  const node = gltf.nodes[ni];
  if (node.mesh === undefined) continue;
  const mesh = gltf.meshes[node.mesh];
  const nodeName = sane(node.name || `node${ni}`);
  const t = node.translation || [0, 0, 0];
  const r = node.rotation || [0, 0, 0, 1];   // xyzw
  const s = node.scale || [1, 1, 1];

  usda += `    def Xform "${nodeName}"
    {
        double3 xformOp:translate = (${f(t[0])}, ${f(t[1])}, ${f(t[2])})
        quatf xformOp:orient = (${f(r[3])}, ${f(r[0])}, ${f(r[1])}, ${f(r[2])})
        float3 xformOp:scale = (${f(s[0])}, ${f(s[1])}, ${f(s[2])})
        uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:orient", "xformOp:scale"]

`;
  mesh.prims = mesh.primitives;
  mesh.primitives.forEach((prim, pi) => {
    const pos = readAccessor(prim.attributes.POSITION);
    const nrm = prim.attributes.NORMAL !== undefined ? readAccessor(prim.attributes.NORMAL) : null;
    const uv = prim.attributes.TEXCOORD_0 !== undefined ? readAccessor(prim.attributes.TEXCOORD_0) : null;
    const idx = readAccessor(prim.indices);
    const triCount = idx.length / 3;

    const pts = [];
    for (let i = 0; i < pos.length; i += 3) pts.push(`(${f(pos[i])}, ${f(pos[i + 1])}, ${f(pos[i + 2])})`);
    const nrms = [];
    if (nrm) for (let i = 0; i < nrm.length; i += 3) nrms.push(`(${f(nrm[i])}, ${f(nrm[i + 1])}, ${f(nrm[i + 2])})`);
    const sts = [];
    if (uv) for (let i = 0; i < uv.length; i += 2) sts.push(`(${f(uv[i])}, ${f(1 - uv[i + 1])})`); // flip V for USD

    const meshName = mesh.primitives.length > 1 ? `${nodeName}_p${pi}` : `${nodeName}_mesh`;
    usda += `        def Mesh "${meshName}"
        {
            uniform bool doubleSided = 1
            int[] faceVertexCounts = [${new Array(triCount).fill(3).join(', ')}]
            int[] faceVertexIndices = [${Array.from(idx).join(', ')}]
            point3f[] points = [${pts.join(', ')}]
`;
    if (nrms.length) {
      usda += `            normal3f[] normals = [${nrms.join(', ')}] (
                interpolation = "vertex"
            )
`;
    }
    if (sts.length) {
      usda += `            texCoord2f[] primvars:st = [${sts.join(', ')}] (
                interpolation = "vertex"
            )
`;
    }
    if (texFile) {
      usda += `            rel material:binding = </${baseName}/Materials/colormap>
`;
    }
    usda += `            uniform token subdivisionScheme = "none"
        }

`;
  });
  usda += `    }

`;
}

usda += `}
`;

const outFile = path.join(outDir, `${baseName}.usda`);
fs.writeFileSync(outFile, usda);
console.log(`wrote ${outFile}${texFile ? ' + ' + texFile : ''} (${sceneNodes.length} nodes)`);
