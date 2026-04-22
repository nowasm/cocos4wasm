#ifdef USE_TINYUSDZ

#include "game/USDLoader.h"
#include "3d/misc/CreateMesh.h"
#include "game/MaterialFactory.h"
#include "game/TextureLoader.h"
#include "primitive/PrimitiveDefine.h"
#include "base/Log.h"
#include "math/Quaternion.h"
#include "math/Vec3.h"

#include "tinyusdz.hh"
#include "tydra/render-data.hh"

#include <cmath>

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

    IGeometry geo;
    if (hasNormals) geo.normals.emplace();
    if (hasUVs)     geo.uvs.emplace();

    const VertexAttribute* uvAttr = hasUVs ? &src.texcoords.at(0) : nullptr;

    size_t gfv = 0;  // global face-vertex offset
    for (size_t f = 0; f < fvCounts.size(); ++f) {
        uint32_t nv = fvCounts[f];
        for (uint32_t tri = 1; tri + 1 < nv; ++tri) {
            size_t fv[3] = { gfv, gfv + tri, gfv + tri + 1 };
            for (int v = 0; v < 3; ++v) {
                uint32_t pi = fvIdx[fv[v]];
                // float3 = std::array<float,3>
                geo.positions.push_back(src.points[pi][0]);
                geo.positions.push_back(src.points[pi][1]);
                geo.positions.push_back(src.points[pi][2]);

                if (hasNormals) {
                    const float* n = attrFloat(src.normals, attrDataIdx(src.normals, fv[v], pi), 3);
                    geo.normals->push_back(n[0]);
                    geo.normals->push_back(n[1]);
                    geo.normals->push_back(n[2]);
                }
                if (hasUVs) {
                    const float* uv = attrFloat(*uvAttr, attrDataIdx(*uvAttr, fv[v], pi), 2);
                    geo.uvs->push_back(uv[0]);
                    geo.uvs->push_back(uv[1]);
                }
            }
        }
        gfv += nv;
    }

    if (geo.positions.empty()) return nullptr;
    ICreateMeshOptions opts;
    opts.calculateBounds = true;
    return MeshUtils::createMesh(geo, nullptr, opts);
}

// ─── Material conversion ─────────────────────────────────────────────────────

Material* buildMaterial(const RenderMaterial& mat,
                         const RenderScene& rs,
                         std::vector<Material*>& out) {
    const auto& s = mat.surfaceShader;

    // Attempt to load diffuse texture from a decoded RGBA buffer
    if (s.diffuseColor.texture_id >= 0) {
        for (const auto& img : rs.images) {
            if (img.decoded && img.buffer_id >= 0 &&
                img.buffer_id < (int)rs.buffers.size() &&
                img.channels == 4 && img.width > 0 && img.height > 0) {
                const auto& buf = rs.buffers[img.buffer_id];
                auto* tex = TextureLoader::createFromRGBA(
                    buf.data.data(), uint32_t(img.width), uint32_t(img.height));
                if (tex) {
                    auto* m = MaterialFactory::createStandardTextured(tex);
                    out.push_back(m);
                    return m;
                }
            }
        }
        // Fall back to loading by file path
        for (const auto& img : rs.images) {
            if (!img.asset_identifier.empty()) {
                auto* tex = TextureLoader::loadFromFile(img.asset_identifier);
                if (tex) {
                    auto* m = MaterialFactory::createStandardTextured(tex);
                    out.push_back(m);
                    return m;
                }
            }
        }
    }

    // Solid colour from UsdPreviewSurface diffuseColor
    // vec3 = std::array<float,3>
    PBRParams params;
    params.albedo    = Color(uint8_t(s.diffuseColor.value[0] * 255.f),
                              uint8_t(s.diffuseColor.value[1] * 255.f),
                              uint8_t(s.diffuseColor.value[2] * 255.f), 255);
    params.roughness = s.roughness.value;
    params.metallic  = s.metallic.value;
    auto* m = MaterialFactory::createStandard(params);
    out.push_back(m);
    return m;
}

// ─── Node tree ───────────────────────────────────────────────────────────────

void buildNodeTree(const tydra::Node& usdNode,
                    cc::Node* parent,
                    const RenderScene& rs,
                    const std::vector<Mesh*>& builtMeshes,
                    std::vector<Material*>& outMats,
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
                mat = buildMaterial(rs.materials[rm.material_id], rs, outMats);
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
            auto* renderer = ccnew MeshRenderer(node);
            renderer->setMesh(mesh);
            renderer->setMaterial(mat);
            result.renderers.push_back(renderer);
        }
    }

    for (const auto& child : usdNode.children) {
        buildNodeTree(child, node, rs, builtMeshes, outMats, result);
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

    tydra::RenderSceneConverterEnv env(stage);
    tydra::RenderSceneConverter converter;
    tydra::RenderScene rs;
    if (!converter.ConvertToRenderScene(env, &rs)) {
        result.error = "ConvertToRenderScene failed: " + filePath;
        CC_LOG_ERROR("USDLoader: %s", result.error.c_str());
        return result;
    }

    // Pre-build all meshes indexed by their id.
    result.meshes.resize(rs.meshes.size(), nullptr);
    for (size_t i = 0; i < rs.meshes.size(); ++i) {
        result.meshes[i] = buildMesh(rs.meshes[i]);
    }

    // Scene root node
    auto* root = ccnew cc::Node(filePath.empty() ? "usd_root" : filePath);
    root->setParent(parent);
    result.nodes.push_back(root);
    result.rootNode = root;

    // rs.nodes = flat list of root-level USD nodes; children are nested.
    for (const auto& usdNode : rs.nodes) {
        buildNodeTree(usdNode, root, rs, result.meshes, result.materials, result);
    }

    result.success = true;
    return result;
}

} // namespace cc::game

#endif // USE_TINYUSDZ
