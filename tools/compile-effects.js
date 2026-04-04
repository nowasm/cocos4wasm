#!/usr/bin/env node
/**
 * compile-effects.js — Compile builtin .effect files into JSON for standalone WASM runtime.
 *
 * Usage:
 *   node tools/compile-effects.js --cocos-cli <path-to-cocos-cli> [--output <output-dir>]
 *
 * Bypasses cc/preload entirely by injecting offline-mappings directly.
 */

'use strict';

const ps = require('path');
const fs = require('fs');
const Module = require('module');

// ---------- Parse CLI args ----------
let cocosCliRoot = '';
let outputDir = '';
const argv = process.argv;
for (let i = 2; i < argv.length; i++) {
    if (argv[i] === '--cocos-cli' && i + 1 < argv.length) {
        cocosCliRoot = ps.resolve(argv[++i]);
    } else if (argv[i] === '--output' && i + 1 < argv.length) {
        outputDir = ps.resolve(argv[++i]);
    }
}

if (!cocosCliRoot) {
    console.error('Usage: node tools/compile-effects.js --cocos-cli <path-to-cocos-cli> [--output <dir>]');
    process.exit(1);
}

const engineRoot = ps.join(cocosCliRoot, 'packages', 'engine');
if (!outputDir) {
    outputDir = ps.join(ps.dirname(__dirname), 'templates', 'wasm32');
}

// ---------- Build a standalone offline-mappings without Creator engine ----------
// All values are numeric GFX enums matching cocos/gfx/base/Define.h

const Type = {
    UNKNOWN: 0, BOOL: 1, BOOL2: 2, BOOL3: 3, BOOL4: 4,
    INT: 5, INT2: 6, INT3: 7, INT4: 8,
    UINT: 9, UINT2: 10, UINT3: 11, UINT4: 12,
    FLOAT: 13, FLOAT2: 14, FLOAT3: 15, FLOAT4: 16,
    MAT2: 17, MAT2X3: 18, MAT2X4: 19, MAT3X2: 20,
    MAT3: 21, MAT3X4: 22, MAT4X2: 23, MAT4X3: 24, MAT4: 25,
    SAMPLER1D: 26, SAMPLER1D_ARRAY: 27, SAMPLER2D: 28, SAMPLER2D_ARRAY: 29,
    SAMPLER3D: 30, SAMPLER_CUBE: 31,
    SAMPLER: 32,
    TEXTURE1D: 33, TEXTURE1D_ARRAY: 34, TEXTURE2D: 35, TEXTURE2D_ARRAY: 36,
    TEXTURE3D: 37, TEXTURE_CUBE: 38,
    IMAGE1D: 39, IMAGE1D_ARRAY: 40, IMAGE2D: 41, IMAGE2D_ARRAY: 42,
    IMAGE3D: 43, IMAGE_CUBE: 44,
    SUBPASS_INPUT: 45, COUNT: 46,
};

const Format = {
    UNKNOWN: 0,
    R8: 35, RG8: 36, RGB8: 37, RGBA8: 38,
    R8I: 43, RG8I: 44, RGB8I: 45, RGBA8I: 46,
    R8UI: 47, RG8UI: 48, RGB8UI: 49, RGBA8UI: 50,
    R16I: 55, RG16I: 56, RGB16I: 57, RGBA16I: 58,
    R16UI: 59, RG16UI: 60, RGB16UI: 61, RGBA16UI: 62,
    R16F: 63, RG16F: 64, RGB16F: 65, RGBA16F: 66,
    R32I: 15, RG32I: 16, RGB32I: 17, RGBA32I: 18,
    R32UI: 19, RG32UI: 20, RGB32UI: 21, RGBA32UI: 22,
    R32F: 23, RG32F: 24, RGB32F: 25, RGBA32F: 26,
};

// FormatInfos - minimal, just needs type field for isNormalized check
const FormatType = { NONE: 0, UNORM: 1, SNORM: 2, UINT: 3, INT: 4, UFLOAT: 5, FLOAT: 6 };
const FormatInfos = {};
// UNORM formats
[35,36,37,38].forEach(f => FormatInfos[f] = { type: FormatType.UNORM });
// INT formats
[43,44,45,46,55,56,57,58,15,16,17,18].forEach(f => FormatInfos[f] = { type: FormatType.INT });
// UINT formats
[47,48,49,50,59,60,61,62,19,20,21,22].forEach(f => FormatInfos[f] = { type: FormatType.UINT });
// FLOAT formats
[63,64,65,66,23,24,25,26].forEach(f => FormatInfos[f] = { type: FormatType.FLOAT });

const ShaderStageFlagBit = { NONE: 0, VERTEX: 1, CONTROL: 2, EVALUATION: 4, GEOMETRY: 8, FRAGMENT: 16, COMPUTE: 32, ALL: 63 };
const DescriptorType = { UNKNOWN: 0, UNIFORM_BUFFER: 1, DYNAMIC_UNIFORM_BUFFER: 2, SAMPLER_TEXTURE: 3, SAMPLER: 4, TEXTURE: 5, STORAGE_BUFFER: 6, DYNAMIC_STORAGE_BUFFER: 7, INPUT_ATTACHMENT: 8, STORAGE_IMAGE: 9 };
const MemoryAccessBit = { NONE: 0, READ_ONLY: 1, WRITE_ONLY: 2, READ_WRITE: 3 };

const ColorMask = { NONE: 0, R: 1, G: 2, B: 4, A: 8, ALL: 15 };
const BlendFactor = { ZERO: 0, ONE: 1, SRC_ALPHA: 2, DST_ALPHA: 3, ONE_MINUS_SRC_ALPHA: 4, ONE_MINUS_DST_ALPHA: 5, SRC_COLOR: 6, DST_COLOR: 7, ONE_MINUS_SRC_COLOR: 8, ONE_MINUS_DST_COLOR: 9, SRC_ALPHA_SATURATE: 10, CONSTANT_COLOR: 11, ONE_MINUS_CONSTANT_COLOR: 12, CONSTANT_ALPHA: 13, ONE_MINUS_CONSTANT_ALPHA: 14 };
const BlendOp = { ADD: 0, SUB: 1, REV_SUB: 2, MIN: 3, MAX: 4 };
const StencilOp = { ZERO: 0, KEEP: 1, REPLACE: 2, INCR: 3, DECR: 4, INVERT: 5, INCR_WRAP: 6, DECR_WRAP: 7 };
const ComparisonFunc = { NEVER: 0, LESS: 1, EQUAL: 2, LESS_EQUAL: 3, GREATER: 4, NOT_EQUAL: 5, GREATER_EQUAL: 6, ALWAYS: 7 };
const CullMode = { NONE: 0, FRONT: 1, BACK: 2 };
const ShadeModel = { GOURAND: 0, FLAT: 1 };
const PolygonMode = { FILL: 0, LINE: 1, POINT: 2 };
const PrimitiveMode = { POINT_LIST: 0, LINE_LIST: 1, LINE_STRIP: 2, LINE_LOOP: 3, TRIANGLE_LIST: 4, TRIANGLE_STRIP: 5, TRIANGLE_FAN: 6, LINE_LIST_ADJACENCY: 7, LINE_STRIP_ADJACENCY: 8, TRIANGLE_LIST_ADJACENCY: 9, TRIANGLE_STRIP_ADJACENCY: 10, TRIANGLE_PATCH_ADJACENCY: 11, QUAD_PATCH_LIST: 12, ISO_LINE_LIST: 13 };
const Filter = { NONE: 0, POINT: 1, LINEAR: 2, ANISOTROPIC: 3 };
const Address = { WRAP: 0, MIRROR: 1, CLAMP: 2, BORDER: 3 };
const DynamicStateFlagBit = { NONE: 0, LINE_WIDTH: 1, DEPTH_BIAS: 2, BLEND_CONSTANTS: 4, DEPTH_BOUNDS: 8, STENCIL_WRITE_MASK: 16, STENCIL_COMPARE_MASK: 32 };

const RenderPassStage = { DEFAULT: 100 };
const RenderPriority = { MIN: 0, MAX: 255, DEFAULT: 128 };
const SetIndex = { GLOBAL: 0, MATERIAL: 1, LOCAL: 2 };

// GetTypeSize - size in bytes for each Type
const typeSizes = {
    [Type.BOOL]: 4, [Type.BOOL2]: 8, [Type.BOOL3]: 12, [Type.BOOL4]: 16,
    [Type.INT]: 4, [Type.INT2]: 8, [Type.INT3]: 12, [Type.INT4]: 16,
    [Type.UINT]: 4, [Type.UINT2]: 8, [Type.UINT3]: 12, [Type.UINT4]: 16,
    [Type.FLOAT]: 4, [Type.FLOAT2]: 8, [Type.FLOAT3]: 12, [Type.FLOAT4]: 16,
    [Type.MAT2]: 16, [Type.MAT2X3]: 24, [Type.MAT2X4]: 32,
    [Type.MAT3X2]: 24, [Type.MAT3]: 36, [Type.MAT3X4]: 48,
    [Type.MAT4X2]: 32, [Type.MAT4X3]: 48, [Type.MAT4]: 64,
};
function GetTypeSize(type) { return typeSizes[type] || 0; }

// murmurhash2_32_gc
function murmurhash2_32_gc(str, seed) {
    let l = str.length, h = seed ^ l, i = 0, k;
    while (l >= 4) {
        k = ((str.charCodeAt(i) & 0xff)) | ((str.charCodeAt(i+1) & 0xff) << 8) |
            ((str.charCodeAt(i+2) & 0xff) << 16) | ((str.charCodeAt(i+3) & 0xff) << 24);
        k = (((k & 0xffff) * 0x5bd1e995) + ((((k >>> 16) * 0x5bd1e995) & 0xffff) << 16));
        k ^= k >>> 24;
        k = (((k & 0xffff) * 0x5bd1e995) + ((((k >>> 16) * 0x5bd1e995) & 0xffff) << 16));
        h = (((h & 0xffff) * 0x5bd1e995) + ((((h >>> 16) * 0x5bd1e995) & 0xffff) << 16)) ^ k;
        l -= 4; i += 4;
    }
    switch (l) {
        case 3: h ^= (str.charCodeAt(i+2) & 0xff) << 16; // fall through
        case 2: h ^= (str.charCodeAt(i+1) & 0xff) << 8;  // fall through
        case 1: h ^= (str.charCodeAt(i) & 0xff);
                h = (((h & 0xffff) * 0x5bd1e995) + ((((h >>> 16) * 0x5bd1e995) & 0xffff) << 16));
    }
    h ^= h >>> 13;
    h = (((h & 0xffff) * 0x5bd1e995) + ((((h >>> 16) * 0x5bd1e995) & 0xffff) << 16));
    h ^= h >>> 15;
    return h >>> 0;
}

// SamplerInfo class
class SamplerInfo {
    constructor() {
        this.minFilter = Filter.LINEAR;
        this.magFilter = Filter.LINEAR;
        this.mipFilter = Filter.NONE;
        this.addressU = Address.WRAP;
        this.addressV = Address.WRAP;
        this.addressW = Address.WRAP;
        this.maxAnisotropy = 0;
        this.cmpFunc = ComparisonFunc.ALWAYS;
    }
}

// Sampler.computeHash
class Sampler {
    static computeHash(info) {
        let h = (info.minFilter || 0);
        h |= ((info.magFilter || 0) << 2);
        h |= ((info.mipFilter || 0) << 4);
        h |= ((info.addressU || 0) << 6);
        h |= ((info.addressV || 0) << 8);
        h |= ((info.addressW || 0) << 10);
        h |= ((info.maxAnisotropy || 0) << 12);
        h |= ((info.cmpFunc || 0) << 16);
        return h;
    }
}

// Build typeMap same as offline-mappings.ts
const typeMap = {};
typeMap[typeMap.bool = Type.BOOL] = 'bool';
typeMap[typeMap.bvec2 = Type.BOOL2] = 'bvec2';
typeMap[typeMap.bvec3 = Type.BOOL3] = 'bvec3';
typeMap[typeMap.bvec4 = Type.BOOL4] = 'bvec4';
typeMap[typeMap.int = Type.INT] = 'int';
typeMap[typeMap.ivec2 = Type.INT2] = 'ivec2';
typeMap[typeMap.ivec3 = Type.INT3] = 'ivec3';
typeMap[typeMap.ivec4 = Type.INT4] = 'ivec4';
typeMap[typeMap.uint = Type.UINT] = 'uint';
typeMap[typeMap.uvec2 = Type.UINT2] = 'uvec2';
typeMap[typeMap.uvec3 = Type.UINT3] = 'uvec3';
typeMap[typeMap.uvec4 = Type.UINT4] = 'uvec4';
typeMap[typeMap.float = Type.FLOAT] = 'float';
typeMap[typeMap.vec2 = Type.FLOAT2] = 'vec2';
typeMap[typeMap.vec3 = Type.FLOAT3] = 'vec3';
typeMap[typeMap.vec4 = Type.FLOAT4] = 'vec4';
typeMap[typeMap.mat2 = Type.MAT2] = 'mat2';
typeMap[typeMap.mat3 = Type.MAT3] = 'mat3';
typeMap[typeMap.mat4 = Type.MAT4] = 'mat4';
typeMap[typeMap.mat2x3 = Type.MAT2X3] = 'mat2x3';
typeMap[typeMap.mat2x4 = Type.MAT2X4] = 'mat2x4';
typeMap[typeMap.mat3x2 = Type.MAT3X2] = 'mat3x2';
typeMap[typeMap.mat3x4 = Type.MAT3X4] = 'mat3x4';
typeMap[typeMap.mat4x2 = Type.MAT4X2] = 'mat4x2';
typeMap[typeMap.mat4x3 = Type.MAT4X3] = 'mat4x3';
typeMap[typeMap.sampler1D = Type.SAMPLER1D] = 'sampler1D';
typeMap[typeMap.sampler1DArray = Type.SAMPLER1D_ARRAY] = 'sampler1DArray';
typeMap[typeMap.sampler2D = Type.SAMPLER2D] = 'sampler2D';
typeMap[typeMap.sampler2DArray = Type.SAMPLER2D_ARRAY] = 'sampler2DArray';
typeMap[typeMap.sampler3D = Type.SAMPLER3D] = 'sampler3D';
typeMap[typeMap.samplerCube = Type.SAMPLER_CUBE] = 'samplerCube';
typeMap[typeMap.sampler = Type.SAMPLER] = 'sampler';
typeMap[typeMap.texture1D = Type.TEXTURE1D] = 'texture1D';
typeMap[typeMap.texture1DArray = Type.TEXTURE1D_ARRAY] = 'texture1DArray';
typeMap[typeMap.texture2D = Type.TEXTURE2D] = 'texture2D';
typeMap[typeMap.texture2DArray = Type.TEXTURE2D_ARRAY] = 'texture2DArray';
typeMap[typeMap.texture3D = Type.TEXTURE3D] = 'texture3D';
typeMap[typeMap.textureCube = Type.TEXTURE_CUBE] = 'textureCube';
typeMap[typeMap.image1D = Type.IMAGE1D] = 'image1D';
typeMap[typeMap.image1DArray = Type.IMAGE1D_ARRAY] = 'image1DArray';
typeMap[typeMap.image2D = Type.IMAGE2D] = 'image2D';
typeMap[typeMap.image2DArray = Type.IMAGE2D_ARRAY] = 'image2DArray';
typeMap[typeMap.image3D = Type.IMAGE3D] = 'image3D';
typeMap[typeMap.imageCube = Type.IMAGE_CUBE] = 'imageCube';
typeMap[typeMap.subpassInput = Type.SUBPASS_INPUT] = 'subpassInput';
// Variations
typeMap.int8_t = Type.INT; typeMap.i8vec2 = Type.INT2; typeMap.i8vec3 = Type.INT3; typeMap.i8vec4 = Type.INT4;
typeMap.uint8_t = Type.UINT; typeMap.u8vec2 = Type.UINT2; typeMap.u8vec3 = Type.UINT3; typeMap.u8vec4 = Type.UINT4;
typeMap.int16_t = Type.INT; typeMap.i16vec2 = Type.INT2; typeMap.i16vec3 = Type.INT3; typeMap.i16vec4 = Type.INT4;
typeMap.uint16_t = Type.INT; typeMap.u16vec2 = Type.UINT2; typeMap.u16vec3 = Type.UINT3; typeMap.u16vec4 = Type.UINT4;
typeMap.float16_t = Type.FLOAT; typeMap.f16vec2 = Type.FLOAT2; typeMap.f16vec3 = Type.FLOAT3; typeMap.f16vec4 = Type.FLOAT4;
typeMap.mat2x2 = Type.MAT2; typeMap.mat3x3 = Type.MAT3; typeMap.mat4x4 = Type.MAT4;
typeMap.isampler1D = Type.SAMPLER1D; typeMap.usampler1D = Type.SAMPLER1D; typeMap.sampler1DShadow = Type.SAMPLER1D;
typeMap.isampler1DArray = Type.SAMPLER1D_ARRAY; typeMap.usampler1DArray = Type.SAMPLER1D_ARRAY; typeMap.sampler1DArrayShadow = Type.SAMPLER1D_ARRAY;
typeMap.isampler2D = Type.SAMPLER2D; typeMap.usampler2D = Type.SAMPLER2D; typeMap.sampler2DShadow = Type.SAMPLER2D;
typeMap.isampler2DArray = Type.SAMPLER2D_ARRAY; typeMap.usampler2DArray = Type.SAMPLER2D_ARRAY; typeMap.sampler2DArrayShadow = Type.SAMPLER2D_ARRAY;
typeMap.isampler3D = Type.SAMPLER3D; typeMap.usampler3D = Type.SAMPLER3D;
typeMap.isamplerCube = Type.SAMPLER_CUBE; typeMap.usamplerCube = Type.SAMPLER_CUBE; typeMap.samplerCubeShadow = Type.SAMPLER_CUBE;
typeMap.iimage2D = Type.IMAGE2D; typeMap.uimage2D = Type.IMAGE2D;
typeMap.usubpassInput = Type.SUBPASS_INPUT; typeMap.isubpassInput = Type.SUBPASS_INPUT;

const formatMap = {
    bool: Format.R8, bvec2: Format.RG8, bvec3: Format.RGB8, bvec4: Format.RGBA8,
    int: Format.R32I, ivec2: Format.RG32I, ivec3: Format.RGB32I, ivec4: Format.RGBA32I,
    uint: Format.R32UI, uvec2: Format.RG32UI, uvec3: Format.RGB32UI, uvec4: Format.RGBA32UI,
    float: Format.R32F, vec2: Format.RG32F, vec3: Format.RGB32F, vec4: Format.RGBA32F,
    int8_t: 43, i8vec2: 44, i8vec3: 45, i8vec4: 46,
    uint8_t: 47, u8vec2: 48, u8vec3: 49, u8vec4: 50,
    int16_t: 55, i16vec2: 56, i16vec3: 57, i16vec4: 58,
    uint16_t: 59, u16vec2: 60, u16vec3: 61, u16vec4: 62,
    float16_t: 63, f16vec2: 64, f16vec3: 65, f16vec4: 66,
    mat2: Format.RGBA32F, mat3: Format.RGBA32F, mat4: Format.RGBA32F,
    mat2x2: Format.RGBA32F, mat3x3: Format.RGBA32F, mat4x4: Format.RGBA32F,
    mat2x3: Format.RGBA32F, mat2x4: Format.RGBA32F,
    mat3x2: Format.RGBA32F, mat3x4: Format.RGBA32F,
    mat4x2: Format.RGBA32F, mat4x3: Format.RGBA32F,
};

const isSampler = (type) => type >= Type.SAMPLER1D;
const isPaddedMatrix = (type) => type >= Type.MAT2 && type < Type.MAT4;
const getFormat = (name) => Format[name.toUpperCase()];
const getShaderStage = (name) => ShaderStageFlagBit[name.toUpperCase()];
const getDescriptorType = (name) => DescriptorType[name.toUpperCase()];
const isNormalized = (format) => {
    const info = FormatInfos[format];
    return info && (info.type === FormatType.UNORM || info.type === FormatType.SNORM);
};
const getMemoryAccessFlag = (access) => {
    if (access === 'writeonly') return MemoryAccessBit.WRITE_ONLY;
    if (access === 'readonly') return MemoryAccessBit.READ_ONLY;
    return MemoryAccessBit.READ_WRITE;
};

const passParams = {
    NONE: ColorMask.NONE, R: ColorMask.R, G: ColorMask.G, B: ColorMask.B, A: ColorMask.A,
    RG: ColorMask.R | ColorMask.G, ALL: ColorMask.ALL,
    ADD: BlendOp.ADD, SUB: BlendOp.SUB, REV_SUB: BlendOp.REV_SUB, MIN: BlendOp.MIN, MAX: BlendOp.MAX,
    ZERO: BlendFactor.ZERO, ONE: BlendFactor.ONE,
    SRC_ALPHA: BlendFactor.SRC_ALPHA, DST_ALPHA: BlendFactor.DST_ALPHA,
    ONE_MINUS_SRC_ALPHA: BlendFactor.ONE_MINUS_SRC_ALPHA, ONE_MINUS_DST_ALPHA: BlendFactor.ONE_MINUS_DST_ALPHA,
    SRC_COLOR: BlendFactor.SRC_COLOR, DST_COLOR: BlendFactor.DST_COLOR,
    ONE_MINUS_SRC_COLOR: BlendFactor.ONE_MINUS_SRC_COLOR, ONE_MINUS_DST_COLOR: BlendFactor.ONE_MINUS_DST_COLOR,
    SRC_ALPHA_SATURATE: BlendFactor.SRC_ALPHA_SATURATE,
    CONSTANT_COLOR: BlendFactor.CONSTANT_COLOR, ONE_MINUS_CONSTANT_COLOR: BlendFactor.ONE_MINUS_CONSTANT_COLOR,
    CONSTANT_ALPHA: BlendFactor.CONSTANT_ALPHA, ONE_MINUS_CONSTANT_ALPHA: BlendFactor.ONE_MINUS_CONSTANT_ALPHA,
    KEEP: StencilOp.KEEP, REPLACE: StencilOp.REPLACE, INCR: StencilOp.INCR, DECR: StencilOp.DECR,
    INVERT: StencilOp.INVERT, INCR_WRAP: StencilOp.INCR_WRAP, DECR_WRAP: StencilOp.DECR_WRAP,
    NEVER: ComparisonFunc.NEVER, LESS: ComparisonFunc.LESS, EQUAL: ComparisonFunc.EQUAL,
    LESS_EQUAL: ComparisonFunc.LESS_EQUAL, GREATER: ComparisonFunc.GREATER, NOT_EQUAL: ComparisonFunc.NOT_EQUAL,
    GREATER_EQUAL: ComparisonFunc.GREATER_EQUAL, ALWAYS: ComparisonFunc.ALWAYS,
    FRONT: CullMode.FRONT, BACK: CullMode.BACK,
    GOURAND: ShadeModel.GOURAND, FLAT: ShadeModel.FLAT,
    FILL: PolygonMode.FILL, LINE: PolygonMode.LINE, POINT: PolygonMode.POINT,
    POINT_LIST: PrimitiveMode.POINT_LIST, LINE_LIST: PrimitiveMode.LINE_LIST, LINE_STRIP: PrimitiveMode.LINE_STRIP,
    LINE_LOOP: PrimitiveMode.LINE_LOOP, TRIANGLE_LIST: PrimitiveMode.TRIANGLE_LIST,
    TRIANGLE_STRIP: PrimitiveMode.TRIANGLE_STRIP, TRIANGLE_FAN: PrimitiveMode.TRIANGLE_FAN,
    LINEAR: Filter.LINEAR, ANISOTROPIC: Filter.ANISOTROPIC,
    WRAP: Address.WRAP, MIRROR: Address.MIRROR, CLAMP: Address.CLAMP, BORDER: Address.BORDER,
    LINE_WIDTH: DynamicStateFlagBit.LINE_WIDTH, DEPTH_BIAS: DynamicStateFlagBit.DEPTH_BIAS,
    BLEND_CONSTANTS: DynamicStateFlagBit.BLEND_CONSTANTS,
    DEPTH_BOUNDS: DynamicStateFlagBit.DEPTH_BOUNDS,
    STENCIL_WRITE_MASK: DynamicStateFlagBit.STENCIL_WRITE_MASK,
    STENCIL_COMPARE_MASK: DynamicStateFlagBit.STENCIL_COMPARE_MASK,
    TRUE: true, FALSE: false,
    DEFAULT: RenderPassStage.DEFAULT,
};

const effectStructure = {
    $techniques: [{ $passes: [{ depthStencilState: {}, rasterizerState: {},
        blendState: { targets: [{}] },
        properties: { any: { sampler: {}, editor: {} } },
        migrations: { properties: { any: {} }, macros: { any: {} } },
        embeddedMacros: {},
    }] }],
};

const offlineMappings = {
    murmurhash2_32_gc, Sampler, SamplerInfo, effectStructure,
    isSampler, typeMap, formatMap, getFormat, getShaderStage, getDescriptorType,
    isNormalized, isPaddedMatrix, getMemoryAccessFlag,
    passParams, SetIndex, RenderPriority, GetTypeSize,
};

// ---------- Hook require to intercept offline-mappings ----------
const originalResolve = Module._resolveFilename;
Module._resolveFilename = function(request, ...args) {
    if (request === './offline-mappings' || request === 'cc/editor/offline-mappings') {
        return '__offline_mappings_injected__';
    }
    return originalResolve.call(this, request, ...args);
};
const originalLoad = Module._cache;
require.cache['__offline_mappings_injected__'] = {
    id: '__offline_mappings_injected__',
    filename: '__offline_mappings_injected__',
    loaded: true,
    exports: offlineMappings,
};

// ---------- Now load shdc-lib ----------
const shdcLib = require(ps.join(cocosCliRoot, 'src', 'core', 'assets', 'effect-compiler', 'shdc-lib'));
shdcLib.options.throwOnWarning = false;
shdcLib.options.throwOnError = false;
shdcLib.options.skipParserTest = true;

// Set up chunk resolution
const chunksDir = ps.join(engineRoot, 'editor', 'assets', 'chunks');
shdcLib.options.chunkSearchFn = (names) => {
    const res = { name: undefined, content: undefined };
    for (let i = 0; i < names.length; i++) {
        const file = ps.resolve(chunksDir, names[i] + '.chunk');
        if (fs.existsSync(file)) {
            res.name = names[i];
            res.content = fs.readFileSync(file, 'utf8');
            break;
        }
    }
    return res;
};

// Add all chunks recursively
const addChunksFromDir = (dir) => {
    if (!fs.existsSync(dir)) return;
    const walk = (d) => {
        for (const entry of fs.readdirSync(d, { withFileTypes: true })) {
            if (entry.isDirectory()) walk(ps.join(d, entry.name));
            else if (entry.name.endsWith('.chunk')) {
                const name = ps.relative(chunksDir, ps.join(d, entry.name)).replace(/\\/g, '/').replace(/\.chunk$/, '');
                shdcLib.addChunk(name, fs.readFileSync(ps.join(d, entry.name), 'utf8'));
            }
        }
    };
    walk(dir);
};
addChunksFromDir(chunksDir);

// Find all .effect files
const effectsRoot = ps.join(engineRoot, 'editor', 'assets', 'effects');
const effectFiles = [];
const walk = (dir) => {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
        const full = ps.join(dir, entry.name);
        if (entry.isDirectory()) walk(full);
        else if (entry.name.endsWith('.effect')) effectFiles.push(full);
    }
};
walk(effectsRoot);

// Compile effects
const compiled = [];
for (const file of effectFiles) {
    const relPath = ps.relative(effectsRoot, ps.dirname(file)).replace(/\\/g, '/');
    const name = (relPath ? relPath + '/' : '') + ps.basename(file, '.effect');
    const content = fs.readFileSync(file, 'utf8');
    try {
        const effect = shdcLib.buildEffect(name, content);
        if (effect) {
            if (effect.editor) delete effect.editor;
            compiled.push(effect);
            console.log(`  [OK] ${name}`);
        } else {
            console.warn(`  [SKIP] ${name}: buildEffect returned null`);
        }
    } catch (e) {
        console.warn(`  [FAIL] ${name}: ${e.message}`);
    }
}

console.log(`\nCompiled ${compiled.length} effects.`);

// Write output
if (!fs.existsSync(outputDir)) fs.mkdirSync(outputDir, { recursive: true });
const outFile = ps.join(outputDir, 'builtin-effects.json');
fs.writeFileSync(outFile, JSON.stringify(compiled), 'utf8');
console.log(`Written to: ${outFile} (${(fs.statSync(outFile).size / 1024).toFixed(1)} KB)`);
