#ifdef USE_TINYUSDZ

#include "game/USDLoader.h"
#include "3d/misc/CreateMesh.h"
#include "3d/framework/SkinnedMeshRendererComponent.h"
#include "game/MaterialFactory.h"
#include "game/TextureLoader.h"
#include "primitive/PrimitiveDefine.h"
#include "base/Log.h"
#include "math/Mat4.h"
#include "math/Quaternion.h"
#include "math/Vec3.h"
#include "renderer/gfx-base/GFXDef-common.h"

#include "tinyusdz.hh"
#include "tydra/render-data.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>

namespace cc::game {
namespace {

using namespace tinyusdz;
using namespace tinyusdz::tydra;

// ─── Transform ───────────────────────────────────────────────────────────────
// USD stores matrices row-major with row-vector convention: v' = v * M.
// Translation lives in row 3. Scale comes from row magnitudes.
// Quaternion must be extracted from M_OpenGL = M_USD^T.

void applyLocalMatrix(cc::Node* node, const value::matrix4d& m) {
    float tx = float(m.m[3][0]);
    float ty = float(m.m[3][1]);
    float tz = float(m.m[3][2]);
    node->setPosition(tx, ty, tz);

    // Scale = magnitude of each USD row (= magnitude of each OpenGL column).
    float sx = float(std::sqrt(m.m[0][0]*m.m[0][0] + m.m[0][1]*m.m[0][1] + m.m[0][2]*m.m[0][2]));
    float sy = float(std::sqrt(m.m[1][0]*m.m[1][0] + m.m[1][1]*m.m[1][1] + m.m[1][2]*m.m[1][2]));
    float sz = float(std::sqrt(m.m[2][0]*m.m[2][0] + m.m[2][1]*m.m[2][1] + m.m[2][2]*m.m[2][2]));
    if (sx < 1e-7f) sx = 1.f;
    if (sy < 1e-7f) sy = 1.f;
    if (sz < 1e-7f) sz = 1.f;
    node->setScale(sx, sy, sz);

    // Normalized OpenGL rotation matrix: R_OGL[row][col] = m.m[col][row] / scale[col]
    float r00 = float(m.m[0][0]/sx), r01 = float(m.m[1][0]/sy), r02 = float(m.m[2][0]/sz);
    float r10 = float(m.m[0][1]/sx), r11 = float(m.m[1][1]/sy), r12 = float(m.m[2][1]/sz);
    float r20 = float(m.m[0][2]/sx), r21 = float(m.m[1][2]/sy), r22 = float(m.m[2][2]/sz);

    // Shepperd quaternion from rotation matrix (column-major / OpenGL convention)
    float trace = r00 + r11 + r22;
    cc::Quaternion q;
    if (trace > 0.f) {
        float s = 0.5f / std::sqrt(trace + 1.f);
        q.w = 0.25f / s;
        q.x = (r21 - r12) * s;
        q.y = (r02 - r20) * s;
        q.z = (r10 - r01) * s;
    } else if (r00 > r11 && r00 > r22) {
        float s = 2.f * std::sqrt(1.f + r00 - r11 - r22);
        q.w = (r21 - r12) / s;  q.x = 0.25f * s;
        q.y = (r01 + r10) / s;  q.z = (r02 + r20) / s;
    } else if (r11 > r22) {
        float s = 2.f * std::sqrt(1.f + r11 - r00 - r22);
        q.w = (r02 - r20) / s;  q.x = (r01 + r10) / s;
        q.y = 0.25f * s;        q.z = (r12 + r21) / s;
    } else {
        float s = 2.f * std::sqrt(1.f + r22 - r00 - r11);
        q.w = (r10 - r01) / s;  q.x = (r02 + r20) / s;
        q.y = (r12 + r21) / s;  q.z = 0.25f * s;
    }
    node->setRotation(q);
}

// ─── USD matrix → engine matrix ──────────────────────────────────────────────
// USD stores matrices row-major with ROW-VECTOR convention (v' = v * M):
// basis vectors live in the rows, translation in row 3. The engine Mat4 is
// column-major with COLUMN-VECTOR convention (v' = M * v): basis in the
// columns, translation in m[12..14]. The engine matrix is therefore the
// TRANSPOSE of the USD one — and because transposing a matrix while also
// flipping the storage order (row-major → column-major) cancels out, the
// USD flat array maps 1:1 onto the engine flat array. A plain element copy
// IS the conversion (m.m[3][0..2] → m[12..14] translation checks out).
inline Mat4 mat4FromUSD(const value::matrix4d& m) {
    Mat4 out;
    const double* src = &m.m[0][0];
    for (int i = 0; i < 16; ++i) out.m[i] = float(src[i]);
    return out;
}

inline bool isIdentityMat(const Mat4& m) {
    static const Mat4 kIdentity;
    for (int i = 0; i < 16; ++i) {
        if (std::fabs(m.m[i] - kIdentity.m[i]) > 1e-6f) return false;
    }
    return true;
}

// ─── Vertex attribute sampling ───────────────────────────────────────────────

// Resolve the data element index for a given global face-vertex index and point index.
inline size_t attrDataIdx(const VertexAttribute& attr, size_t globalFvIdx, uint32_t pointIdx) {
    if (attr.variability == VertexVariability::FaceVarying) return globalFvIdx;
    if (attr.variability == VertexVariability::Indexed && !attr.indices.empty()) {
        return attr.indices[globalFvIdx];
    }
    return pointIdx;  // Vertex / Varying / Constant
}

inline const float* attrFloat(const VertexAttribute& attr, size_t dataIdx, int comps) {
    return reinterpret_cast<const float*>(attr.data.data()) + dataIdx * comps;
}

// ─── Mesh conversion ─────────────────────────────────────────────────────────

Mesh* buildMesh(const RenderMesh& src) {
    const auto& fvIdx    = src.faceVertexIndices();
    const auto& fvCounts = src.faceVertexCounts();
    if (fvIdx.empty() || fvCounts.empty() || src.points.empty()) return nullptr;

    bool hasNormals = !src.normals.data.empty();
    bool hasUVs     = src.texcoords.count(0) && !src.texcoords.at(0).data.empty();
    // Tangent-space basis for normal mapping. Tydra computes tangents and
    // binormals (vec3 each) when normals exist; the engine wants vec4 with
    // w = handedness.
    bool hasTangents = hasNormals &&
                       !src.tangents.data.empty() &&
                       !src.binormals.data.empty();

    // UsdSkel: jointIndices/jointWeights are 'vertex'-varying, `elementSize`
    // influences per point. Tydra keeps them aligned with `points` even when
    // build_vertex_indices rewrites the vertex stream.
    const JointAndWeight& jw = src.joint_and_weights;
    const int  elemSize  = jw.elementSize > 0 ? jw.elementSize : 1;
    const bool isSkinned = src.skel_id >= 0 &&
                           !jw.jointIndices.empty() &&
                           jw.jointIndices.size() == jw.jointWeights.size() &&
                           jw.jointIndices.size() >= src.points.size() * size_t(elemSize);

    // primvars:skel:geomBindTransform — bake it into positions/normals so the
    // Skeleton bindposes stay pure inverse-bind matrices.
    Mat4 geomBind = mat4FromUSD(jw.geomBindTransform);
    bool applyGeomBind = isSkinned && !isIdentityMat(geomBind);
    Mat4 geomBindIT;
    if (applyGeomBind) Mat4::inverseTranspose(geomBind, &geomBindIT);

    IGeometry geo;
    if (hasNormals)  geo.normals.emplace();
    if (hasUVs)      geo.uvs.emplace();
    if (hasTangents) geo.tangents.emplace();

    ccstd::vector<float> jointVals;   // 4 per vertex (uint payload in float storage)
    ccstd::vector<float> weightVals;  // 4 per vertex

    const VertexAttribute* uvAttr = hasUVs ? &src.texcoords.at(0) : nullptr;

    size_t gfv = 0;  // global face-vertex offset
    for (size_t f = 0; f < fvCounts.size(); ++f) {
        uint32_t nv = fvCounts[f];
        for (uint32_t tri = 1; tri + 1 < nv; ++tri) {
            size_t fv[3] = { gfv, gfv + tri, gfv + tri + 1 };
            for (int v = 0; v < 3; ++v) {
                uint32_t pi = fvIdx[fv[v]];
                // float3 = std::array<float,3>
                float px = src.points[pi][0];
                float py = src.points[pi][1];
                float pz = src.points[pi][2];
                if (applyGeomBind) {
                    const float* m = geomBind.m;
                    float tx = m[0]*px + m[4]*py + m[8]*pz  + m[12];
                    float ty = m[1]*px + m[5]*py + m[9]*pz  + m[13];
                    float tz = m[2]*px + m[6]*py + m[10]*pz + m[14];
                    px = tx; py = ty; pz = tz;
                }
                geo.positions.push_back(px);
                geo.positions.push_back(py);
                geo.positions.push_back(pz);

                if (hasNormals) {
                    const float* n = attrFloat(src.normals, attrDataIdx(src.normals, fv[v], pi), 3);
                    float nx = n[0], ny = n[1], nz = n[2];
                    if (applyGeomBind) {
                        const float* m = geomBindIT.m;
                        float tx = m[0]*nx + m[4]*ny + m[8]*nz;
                        float ty = m[1]*nx + m[5]*ny + m[9]*nz;
                        float tz = m[2]*nx + m[6]*ny + m[10]*nz;
                        float len = std::sqrt(tx*tx + ty*ty + tz*tz);
                        if (len > 1e-7f) { tx /= len; ty /= len; tz /= len; }
                        nx = tx; ny = ty; nz = tz;
                    }
                    geo.normals->push_back(nx);
                    geo.normals->push_back(ny);
                    geo.normals->push_back(nz);
                }
                if (hasUVs) {
                    const float* uv = attrFloat(*uvAttr, attrDataIdx(*uvAttr, fv[v], pi), 2);
                    geo.uvs->push_back(uv[0]);
                    geo.uvs->push_back(uv[1]);
                }

                if (hasTangents) {
                    const float* t = attrFloat(src.tangents,  attrDataIdx(src.tangents,  fv[v], pi), 3);
                    const float* b = attrFloat(src.binormals, attrDataIdx(src.binormals, fv[v], pi), 3);
                    const float* n = attrFloat(src.normals,   attrDataIdx(src.normals,   fv[v], pi), 3);
                    // w = handedness: does cross(N, T) point along B?
                    const float cx = n[1] * t[2] - n[2] * t[1];
                    const float cy = n[2] * t[0] - n[0] * t[2];
                    const float cz = n[0] * t[1] - n[1] * t[0];
                    const float w = (cx * b[0] + cy * b[1] + cz * b[2]) < 0.f ? -1.f : 1.f;
                    geo.tangents->push_back(t[0]);
                    geo.tangents->push_back(t[1]);
                    geo.tangents->push_back(t[2]);
                    geo.tangents->push_back(w);
                }

                if (isSkinned) {
                    // Collect this point's influences, keep the strongest 4
                    // (a_joints/a_weights are fixed vec4), renormalize.
                    // USD weights are not guaranteed normalized to begin with.
                    struct Influence { int joint; float weight; };
                    ccstd::vector<Influence> inf;
                    inf.reserve(size_t(elemSize));
                    for (int k = 0; k < elemSize; ++k) {
                        size_t at = size_t(pi) * size_t(elemSize) + size_t(k);
                        int   ji = jw.jointIndices[at];
                        float wt = jw.jointWeights[at];
                        if (ji >= 0 && wt > 0.f) inf.push_back({ji, wt});
                    }
                    std::sort(inf.begin(), inf.end(),
                              [](const Influence& a, const Influence& b) { return a.weight > b.weight; });
                    if (inf.size() > 4) inf.resize(4);
                    float sum = 0.f;
                    for (const auto& i : inf) sum += i.weight;
                    for (int k = 0; k < 4; ++k) {
                        if (k < int(inf.size())) {
                            jointVals.push_back(float(inf[k].joint));
                            weightVals.push_back(sum > 0.f ? inf[k].weight / sum : (k == 0 ? 1.f : 0.f));
                        } else {
                            jointVals.push_back(0.f);
                            weightVals.push_back(k == 0 && inf.empty() ? 1.f : 0.f);
                        }
                    }
                }
            }
        }
        gfv += nv;
    }

    if (geo.positions.empty()) return nullptr;

    if (isSkinned) {
        // a_joints is declared `u32vec4` in the glsl4 (Vulkan) variant of the
        // builtin effects, so the vertex data must be a true integer format —
        // RGBA32UI. writeBuffer() casts the float-typed CustomAttribute
        // payload to uint32 per component. a_weights stays float RGBA32F.
        geo.customAttributes.emplace();
        CustomAttribute joints;
        joints.attr = gfx::Attribute{gfx::ATTR_NAME_JOINTS, gfx::Format::RGBA32UI};
        joints.values = std::move(jointVals);
        CustomAttribute weights;
        weights.attr = gfx::Attribute{gfx::ATTR_NAME_WEIGHTS, gfx::Format::RGBA32F};
        weights.values = std::move(weightVals);
        geo.customAttributes->push_back(std::move(joints));
        geo.customAttributes->push_back(std::move(weights));
    }

    ICreateMeshOptions opts;
    opts.calculateBounds = true;
    return MeshUtils::createMesh(geo, nullptr, opts);
}

// ─── Material conversion ─────────────────────────────────────────────────────

// Resolve the TextureImage a UsdPreviewSurface input actually references:
// texture_id indexes rs.textures (UVTexture), which points at rs.images.
const tydra::TextureImage* imageForTexture(const RenderScene& rs, int64_t textureId) {
    if (textureId < 0 || textureId >= (int64_t)rs.textures.size()) return nullptr;
    int64_t imgId = rs.textures[textureId].texture_image_id;
    if (imgId < 0 || imgId >= (int64_t)rs.images.size()) return nullptr;
    return &rs.images[imgId];
}

// Create a Texture2D from a tydra TextureImage, trying in order:
//   1. decoded texel buffer (RGBA8 direct, RGB8 expanded)
//   2. raw encoded bytes in the buffer (PNG/JPG — e.g. USDZ embedded assets)
//   3. the asset path, as-is then relative to the USD file's directory
Texture2D* textureFromImage(const tydra::TextureImage& img,
                             const RenderScene& rs,
                             const std::string& baseDir) {
    if (img.buffer_id >= 0 && img.buffer_id < (int64_t)rs.buffers.size()) {
        const auto& data = rs.buffers[img.buffer_id].data;
        if (!data.empty()) {
            if (img.decoded && img.width > 0 && img.height > 0) {
                const size_t pixels = size_t(img.width) * size_t(img.height);
                if (img.channels == 4 && data.size() >= pixels * 4) {
                    return TextureLoader::createFromRGBA(
                        data.data(), uint32_t(img.width), uint32_t(img.height));
                }
                if (img.channels == 3 && data.size() >= pixels * 3) {
                    std::vector<uint8_t> rgba(pixels * 4);
                    for (size_t i = 0; i < pixels; ++i) {
                        rgba[i * 4 + 0] = data[i * 3 + 0];
                        rgba[i * 4 + 1] = data[i * 3 + 1];
                        rgba[i * 4 + 2] = data[i * 3 + 2];
                        rgba[i * 4 + 3] = 255;
                    }
                    return TextureLoader::createFromRGBA(
                        rgba.data(), uint32_t(img.width), uint32_t(img.height));
                }
            } else if (!img.decoded) {
                if (auto* tex = TextureLoader::loadFromMemory(
                        data.data(), uint32_t(data.size()))) {
                    return tex;
                }
            }
        }
    }
    if (!img.asset_identifier.empty()) {
        if (auto* tex = TextureLoader::loadFromFile(img.asset_identifier)) return tex;
        if (!baseDir.empty()) {
            if (auto* tex = TextureLoader::loadFromFile(
                    baseDir + "/" + img.asset_identifier)) {
                return tex;
            }
        }
    }
    return nullptr;
}

// Keyed by asset identifier — tydra emits one TextureImage per UsdUVTexture,
// so the same file can appear under several image entries (e.g. an ORM
// texture read by both the metallic and roughness inputs).
using TextureCache = std::map<std::string, Texture2D*>;

std::string textureCacheKey(const tydra::TextureImage& img) {
    if (!img.asset_identifier.empty()) return img.asset_identifier;
    // In-memory-only images: fall back to identity via buffer id.
    return "__buffer_" + std::to_string(img.buffer_id);
}

Material* buildMaterial(const RenderMaterial& mat,
                         const RenderScene& rs,
                         const std::string& baseDir,
                         TextureCache& texCache,
                         std::vector<Material*>& out) {
    const auto& s = mat.surfaceShader;

    // Resolve + decode each texture slot once per image (slots often share
    // one image, e.g. metallic and roughness both reading an ORM texture).
    // COCOS_USD_NO_TEX=1 forces the solid-color path (debugging aid).
    const char* noTexEnv = ::getenv("COCOS_USD_NO_TEX");
    const bool skipTextures = noTexEnv && noTexEnv[0] == '1';
    auto texFor = [&](int64_t textureId) -> Texture2D* {
        if (skipTextures) return nullptr;
        const auto* img = imageForTexture(rs, textureId);
        if (!img) return nullptr;
        const std::string key = textureCacheKey(*img);
        auto it = texCache.find(key);
        if (it != texCache.end()) return it->second;
        Texture2D* tex = textureFromImage(*img, rs, baseDir);
        if (!tex) {
            CC_LOG_WARNING("USDLoader: texture '%s' could not be loaded",
                           img->asset_identifier.c_str());
        }
        texCache[key] = tex;  // cache failures too, avoids repeated decode attempts
        return tex;
    };

    StandardTextures textures;
    textures.albedo = texFor(s.diffuseColor.texture_id);
    textures.normal = texFor(s.normal.texture_id);

    // Roughness/metallic usually reference one packed ORM image; builtin-
    // standard samples it as pbrMap (r=ao g=roughness b=metallic).
    const auto* roughImg = imageForTexture(rs, s.roughness.texture_id);
    const auto* metalImg = imageForTexture(rs, s.metallic.texture_id);
    if (roughImg && metalImg &&
        textureCacheKey(*roughImg) != textureCacheKey(*metalImg)) {
        CC_LOG_WARNING("USDLoader: material '%s' uses separate roughness/metallic "
                       "textures; using the roughness one as pbrMap",
                       mat.name.c_str());
    }
    textures.pbr = texFor(s.roughness.texture_id >= 0 ? s.roughness.texture_id
                                                       : s.metallic.texture_id);

    // Separate AO map only when it is a different image — if AO shares the
    // ORM texture, pbrMap.r already covers it.
    const auto* pbrImg = roughImg ? roughImg : metalImg;
    const auto* occImg = imageForTexture(rs, s.occlusion.texture_id);
    if (occImg && (!pbrImg || textureCacheKey(*occImg) != textureCacheKey(*pbrImg))) {
        textures.occlusion = texFor(s.occlusion.texture_id);
    }
    textures.emissive = texFor(s.emissiveColor.texture_id);

    PBRParams params;
    // With an albedo map the color uniform is a multiplier — keep it white.
    params.albedo = textures.albedo
                        ? Color(255, 255, 255, 255)
                        : Color(uint8_t(s.diffuseColor.value[0] * 255.f),
                                uint8_t(s.diffuseColor.value[1] * 255.f),
                                uint8_t(s.diffuseColor.value[2] * 255.f), 255);
    params.roughness = s.roughness.value;
    params.metallic  = s.metallic.value;
    params.emissive  = Color(uint8_t(s.emissiveColor.value[0] * 255.f),
                              uint8_t(s.emissiveColor.value[1] * 255.f),
                              uint8_t(s.emissiveColor.value[2] * 255.f), 255);

    if (textures.albedo || textures.normal || textures.pbr ||
        textures.occlusion || textures.emissive) {
        CC_LOG_INFO("USDLoader: material '%s' maps: albedo=%d normal=%d pbr=%d ao=%d emissive=%d",
                    mat.name.c_str(),
                    textures.albedo != nullptr, textures.normal != nullptr,
                    textures.pbr != nullptr, textures.occlusion != nullptr,
                    textures.emissive != nullptr);
    }

    auto* m = MaterialFactory::createStandardPBR(params, textures);
    out.push_back(m);
    return m;
}

// ─── UsdSkel: joints / skeleton / animation ──────────────────────────────────

// Joint tokens look like "Base/Upper" (path segments relative to the
// Skeleton prim). Occasionally they are authored absolute ("/Base/Upper");
// normalize by dropping the leading slash so Node::getChildByPath works.
inline std::string normalizeJointPath(const std::string& jointPath) {
    if (!jointPath.empty() && jointPath.front() == '/') return jointPath.substr(1);
    return jointPath;
}

inline std::string jointLeafName(const std::string& jointPath) {
    auto pos = jointPath.find_last_of('/');
    return pos == std::string::npos ? jointPath : jointPath.substr(pos + 1);
}

// Recursively mirror the SkelHierarchy as engine Nodes under `parent`.
// Node names are the joint-path leaf segments, so the full joint path is
// resolvable from the skinning root via Node::getChildByPath. Local rest
// transforms come from SkelNode::rest_transform.
void buildJointNodes(const tydra::SkelNode& sn, cc::Node* parent, USDLoadResult& result) {
    std::string leaf = jointLeafName(normalizeJointPath(sn.joint_path));
    auto* jn = ccnew cc::Node(leaf.empty() ? "joint" : leaf);
    jn->setParent(parent);
    result.nodes.push_back(jn);
    applyLocalMatrix(jn, sn.rest_transform);
    for (const auto& child : sn.children) {
        buildJointNodes(child, jn, result);
    }
}

// Flatten the SkelHierarchy into (path, inverse-bind) arrays indexed by
// joint_id — a_joints vertex data indexes the same UsdSkel joint order.
// SkelNode::bind_transform is the joint's WORLD bind matrix (skeleton
// space); the Skeleton asset stores its inverse.
void collectJoints(const tydra::SkelNode& sn,
                   ccstd::vector<ccstd::string>& paths,
                   ccstd::vector<Mat4>& bindposes) {
    if (sn.joint_id >= 0) {
        size_t idx = size_t(sn.joint_id);
        if (paths.size() <= idx) {
            paths.resize(idx + 1);
            bindposes.resize(idx + 1);
        }
        paths[idx]     = normalizeJointPath(sn.joint_path).c_str();
        bindposes[idx] = mat4FromUSD(sn.bind_transform).getInversed();
    }
    for (const auto& child : sn.children) {
        collectJoints(child, paths, bindposes);
    }
}

Skeleton* buildSkeletonAsset(const tydra::SkelHierarchy& skel) {
    ccstd::vector<ccstd::string> paths;
    ccstd::vector<Mat4> bindposes;
    collectJoints(skel.root_node, paths, bindposes);
    if (paths.empty()) return nullptr;
    auto* asset = ccnew Skeleton();
    asset->setJoints(paths);
    asset->setBindposes(bindposes);
    return asset;  // hash is computed lazily from the bindposes
}

// Convert one tydra::Animation (USD SkelAnimation) into an AnimationClip.
// Channel sample times are USD timecodes; divide by timeCodesPerSecond to
// get seconds and rebase so the earliest key sits at t = 0. tydra quats are
// (x, y, z, w) memory order — identical to engine Quaternion.
AnimationClip* convertSkelAnimation(const tydra::Animation& anim, double timeCodesPerSecond) {
    const float fps = timeCodesPerSecond > 0.0 ? float(timeCodesPerSecond) : 24.f;

    // Rebase: find the earliest sample time across all channels.
    float t0 = std::numeric_limits<float>::max();
    for (const auto& jointIt : anim.channels_map) {
        for (const auto& chanIt : jointIt.second) {
            const AnimationChannel& ch = chanIt.second;
            for (const auto& s : ch.translations.samples) t0 = std::min(t0, s.t);
            for (const auto& s : ch.rotations.samples)    t0 = std::min(t0, s.t);
            for (const auto& s : ch.scales.samples)       t0 = std::min(t0, s.t);
        }
    }
    if (t0 == std::numeric_limits<float>::max()) t0 = 0.f;

    auto* clip = ccnew AnimationClip(anim.prim_name.empty() ? "usd_skel_anim" : anim.prim_name);

    auto toSeconds = [fps, t0](float t) { return (t - t0) / fps; };

    for (const auto& jointIt : anim.channels_map) {
        const std::string relPath = normalizeJointPath(jointIt.first);
        auto& track = clip->track(relPath.c_str());

        for (const auto& chanIt : jointIt.second) {
            const AnimationChannel& ch = chanIt.second;
            switch (chanIt.first) {
                case AnimationChannel::ChannelType::Translation: {
                    track.position.interpolation =
                        ch.translations.interpolation == AnimationSampler<vec3>::Interpolation::Step
                            ? anim::Interpolation::STEP : anim::Interpolation::LINEAR;
                    for (const auto& s : ch.translations.samples) {
                        track.position.addKey(toSeconds(s.t), Vec3(s.value[0], s.value[1], s.value[2]));
                    }
                    if (ch.translations.samples.empty() && ch.translations.static_value.has_value()) {
                        const auto& v = ch.translations.static_value.value();
                        track.position.addKey(0.f, Vec3(v[0], v[1], v[2]));
                    }
                    break;
                }
                case AnimationChannel::ChannelType::Rotation: {
                    track.rotation.interpolation =
                        ch.rotations.interpolation == AnimationSampler<quat>::Interpolation::Step
                            ? anim::Interpolation::STEP : anim::Interpolation::LINEAR;
                    for (const auto& s : ch.rotations.samples) {
                        track.rotation.addKey(toSeconds(s.t),
                                              Quaternion(s.value[0], s.value[1], s.value[2], s.value[3]));
                    }
                    if (ch.rotations.samples.empty() && ch.rotations.static_value.has_value()) {
                        const auto& v = ch.rotations.static_value.value();
                        track.rotation.addKey(0.f, Quaternion(v[0], v[1], v[2], v[3]));
                    }
                    break;
                }
                case AnimationChannel::ChannelType::Scale: {
                    track.scale.interpolation =
                        ch.scales.interpolation == AnimationSampler<vec3>::Interpolation::Step
                            ? anim::Interpolation::STEP : anim::Interpolation::LINEAR;
                    for (const auto& s : ch.scales.samples) {
                        track.scale.addKey(toSeconds(s.t), Vec3(s.value[0], s.value[1], s.value[2]));
                    }
                    if (ch.scales.samples.empty() && ch.scales.static_value.has_value()) {
                        const auto& v = ch.scales.static_value.value();
                        track.scale.addKey(0.f, Vec3(v[0], v[1], v[2]));
                    }
                    break;
                }
                case AnimationChannel::ChannelType::Transform:
                    CC_LOG_WARNING("[USDLoader] matrix (Transform) animation channels are not supported "
                                   "(joint '%s') — decompose to TRS on export", jointIt.first.c_str());
                    break;
                default:
                    break;
            }
        }
    }
    return clip;
}

// Wire one skinned RenderMesh: joint node tree + Skeleton asset +
// SkinnedMeshRendererComponent on `node` (which doubles as skinning root),
// plus an AnimationComponent when the skeleton carries a SkelAnimation.
// Skeleton assets and clips are cached per skel/anim id so meshes sharing a
// UsdSkel Skeleton share them (each mesh still gets its own joint tree).
struct SkelCache {
    std::map<int, IntrusivePtr<Skeleton>>      skeletons;
    std::map<int, IntrusivePtr<AnimationClip>> clips;
};

void setupSkinnedMesh(cc::Node* node, Mesh* mesh, Material* mat,
                      const RenderMesh& rm, const RenderScene& rs,
                      SkelCache& cache, USDLoadResult& result) {
    if (rm.skel_id < 0 || rm.skel_id >= int(rs.skeletons.size())) return;
    const tydra::SkelHierarchy& skel = rs.skeletons[size_t(rm.skel_id)];

    // Skeleton asset (shared per skel_id)
    auto skIt = cache.skeletons.find(rm.skel_id);
    if (skIt == cache.skeletons.end()) {
        Skeleton* asset = buildSkeletonAsset(skel);
        if (!asset) {
            CC_LOG_ERROR("[USDLoader] failed to build Skeleton asset for '%s'", skel.abs_path.c_str());
            return;
        }
        skIt = cache.skeletons.emplace(rm.skel_id, asset).first;
        result.skeletons.emplace_back(asset);
    }
    Skeleton* skeletonAsset = skIt->second.get();

    // Joint node tree under the mesh node (= skinning root)
    buildJointNodes(skel.root_node, node, result);

    // Renderer component — SkinningModel is created inside once mesh,
    // skeleton and material are all present.
    auto* renderer = node->addComponent<SkinnedMeshRendererComponent>();
    renderer->setSkinningRoot(node);
    renderer->setSkeleton(skeletonAsset);
    renderer->setMaterial(mat);
    renderer->setMesh(mesh);
    result.skinnedRenderers.push_back(renderer);

    // Animation clip + component (clip shared per anim_id)
    if (skel.anim_id >= 0 && skel.anim_id < int(rs.animations.size())) {
        auto clIt = cache.clips.find(skel.anim_id);
        if (clIt == cache.clips.end()) {
            AnimationClip* clip = convertSkelAnimation(rs.animations[size_t(skel.anim_id)],
                                                       rs.meta.timeCodesPerSecond);
            clIt = cache.clips.emplace(skel.anim_id, clip).first;
            result.animationClips.emplace_back(clip);
        }
        auto* animComp = node->addComponent<AnimationComponent>();
        animComp->addClip(clIt->second.get());
        result.animationComponents.push_back(animComp);
    }

    CC_LOG_INFO("[USDLoader] skinned mesh '%s': %zu joint(s), anim %s",
                rm.prim_name.c_str(), skeletonAsset->getJoints().size(),
                skel.anim_id >= 0 ? "yes" : "no");
}

// ─── Node tree ───────────────────────────────────────────────────────────────

void buildNodeTree(const tydra::Node& usdNode,
                    cc::Node* parent,
                    const RenderScene& rs,
                    const std::vector<Mesh*>& builtMeshes,
                    std::vector<Material*>& outMats,
                    SkelCache& skelCache,
                    const std::string& baseDir,
                    std::map<int, Material*>& matCache,
                    TextureCache& texCache,
                    USDLoadResult& result) {
    const std::string& primName = usdNode.prim_name;
    auto* node = ccnew cc::Node(primName.empty() ? "usd_node" : primName);
    node->setParent(parent);
    result.nodes.push_back(node);

    applyLocalMatrix(node, usdNode.local_matrix);

    if (usdNode.nodeType == NodeType::Mesh &&
        usdNode.id >= 0 && usdNode.id < (int)builtMeshes.size()) {
        Mesh* mesh = builtMeshes[usdNode.id];
        if (mesh) {
            const RenderMesh& rm = rs.meshes[usdNode.id];
            Material* mat = nullptr;
            if (rm.material_id >= 0 && rm.material_id < (int)rs.materials.size()) {
                auto it = matCache.find(rm.material_id);
                if (it != matCache.end()) {
                    mat = it->second;
                } else {
                    mat = buildMaterial(rs.materials[rm.material_id], rs, baseDir, texCache, outMats);
                    matCache[rm.material_id] = mat;
                }
            }
            if (!mat) {
                // Use displayColor as fallback (color3f has .r .g .b)
                PBRParams p;
                p.albedo = Color(uint8_t(rm.displayColor.r * 255.f),
                                  uint8_t(rm.displayColor.g * 255.f),
                                  uint8_t(rm.displayColor.b * 255.f), 255);
                mat = MaterialFactory::createStandard(p);
                outMats.push_back(mat);
            }
            if (rm.skel_id >= 0) {
                // GPU-skinned path: SkinnedMeshRendererComponent + joint tree
                setupSkinnedMesh(node, mesh, mat, rm, rs, skelCache, result);
            } else {
                auto* renderer = ccnew MeshRenderer(node);
                renderer->setMesh(mesh);
                renderer->setMaterial(mat);
                result.renderers.push_back(renderer);
            }
        }
    }

    for (const auto& child : usdNode.children) {
        buildNodeTree(child, node, rs, builtMeshes, outMats, skelCache,
                      baseDir, matCache, texCache, result);
    }
}

} // anonymous namespace

// ─── Public API ──────────────────────────────────────────────────────────────

USDLoadResult USDLoader::load(const std::string& filePath, Node* parent) {
    USDLoadResult result;

    std::string warn, err;
    tinyusdz::Stage stage;
    if (!tinyusdz::LoadUSDFromFile(filePath, &stage, &warn, &err)) {
        result.error = err.empty() ? "LoadUSDFromFile failed: " + filePath : err;
        CC_LOG_ERROR("USDLoader: %s", result.error.c_str());
        return result;
    }
    if (!warn.empty()) CC_LOG_WARNING("USDLoader: %s", warn.c_str());

    // Directory of the USD file — texture asset paths resolve relative to it.
    std::string baseDir;
    if (auto pos = filePath.find_last_of("/\\"); pos != std::string::npos) {
        baseDir = filePath.substr(0, pos);
    }

    tydra::RenderSceneConverterEnv env(stage);
    if (!baseDir.empty()) env.set_search_paths({baseDir});
    // Keep 8/16bit texel data as-is (no fp32 expansion) — the engine samples
    // sRGB textures and converts in-shader; also the recommended tinyusdz
    // config for mobile/WebGL targets.
    env.material_config.preserve_texel_bitdepth = true;

    // USDZ: textures live inside the zip container ("0/albedo.jpg" style
    // paths). Route asset resolution through the archive; the USDZAsset must
    // outlive ConvertToRenderScene.
    tinyusdz::USDZAsset usdzAsset;
    {
        std::string ext;
        if (auto dot = filePath.find_last_of('.'); dot != std::string::npos) {
            ext = filePath.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
        }
        if (ext == "usdz") {
            std::string zwarn, zerr;
            if (tinyusdz::ReadUSDZAssetInfoFromFile(filePath, &usdzAsset, &zwarn, &zerr)) {
                if (!tinyusdz::SetupUSDZAssetResolution(env.asset_resolver, &usdzAsset)) {
                    CC_LOG_WARNING("USDLoader: USDZ asset resolution setup failed: %s",
                                   filePath.c_str());
                }
            } else {
                CC_LOG_WARNING("USDLoader: cannot index USDZ assets: %s",
                               zerr.empty() ? zwarn.c_str() : zerr.c_str());
            }
        }
    }

    tydra::RenderSceneConverter converter;
    tydra::RenderScene rs;
    if (!converter.ConvertToRenderScene(env, &rs)) {
        result.error = "ConvertToRenderScene failed: " + filePath;
        CC_LOG_ERROR("USDLoader: %s", result.error.c_str());
        if (!converter.GetError().empty()) {
            CC_LOG_ERROR("USDLoader: converter error: %s", converter.GetError().c_str());
        }
        return result;
    }
    if (!converter.GetWarning().empty()) {
        CC_LOG_WARNING("USDLoader: converter warning: %s", converter.GetWarning().c_str());
    }
    CC_LOG_INFO("USDLoader: converted %zu mesh(es), %zu material(s), %zu texture(s), %zu image(s)",
                rs.meshes.size(), rs.materials.size(), rs.textures.size(), rs.images.size());

    // Pre-build all meshes indexed by their id.
    result.meshes.resize(rs.meshes.size(), nullptr);
    for (size_t i = 0; i < rs.meshes.size(); ++i) {
        result.meshes[i] = buildMesh(rs.meshes[i]);
    }

    // Scene root node. The result owns exactly one reference to the root;
    // all other nodes are owned by their parents (see USDLoadResult::nodes).
    auto* root = ccnew cc::Node(filePath.empty() ? "usd_root" : filePath);
    root->addRef();
    root->setParent(parent);
    result.nodes.push_back(root);
    result.rootNode = root;

    // rs.nodes = flat list of root-level USD nodes; children are nested.
    SkelCache skelCache;
    std::map<int, Material*> matCache;
    TextureCache texCache;
    for (const auto& usdNode : rs.nodes) {
        buildNodeTree(usdNode, root, rs, result.meshes, result.materials, skelCache,
                      baseDir, matCache, texCache, result);
    }

    result.success = true;
    return result;
}

} // namespace cc::game

#endif // USE_TINYUSDZ
