#include "game/PrimitiveFactory.h"
#include "3d/misc/CreateMesh.h"
#include "core/assets/RenderingSubMesh.h"
#include "primitive/Box.h"
#include "primitive/Sphere.h"
#include "primitive/Plane.h"
#include "primitive/Cylinder.h"
#include "primitive/Cone.h"
#include "primitive/Torus.h"
#include "primitive/Quad.h"

namespace cc::game {

Mesh* PrimitiveFactory::createBox(float width, float height, float length) {
    IBoxOptions opts;
    opts.width = width;
    opts.height = height;
    opts.length = length;
    auto* mesh = ccnew Mesh();
    mesh->reset(MeshUtils::createMeshInfo(box(opts)));
    mesh->initialize();
    return mesh;
}

Mesh* PrimitiveFactory::createSphere(float radius, uint32_t segments) {
    ISphereOptions opts;
    opts.segments = segments;
    auto* mesh = ccnew Mesh();
    mesh->reset(MeshUtils::createMeshInfo(sphere(radius, opts)));
    mesh->initialize();
    return mesh;
}

Mesh* PrimitiveFactory::createPlane(float width, float length) {
    IPlaneOptions opts;
    opts.width = width;
    opts.length = length;
    auto* mesh = ccnew Mesh();
    mesh->reset(MeshUtils::createMeshInfo(plane(opts)));
    mesh->initialize();
    return mesh;
}

Mesh* PrimitiveFactory::createCylinder(float radiusTop, float radiusBottom, float height) {
    ICylinderOptions opts;
    auto* mesh = ccnew Mesh();
    mesh->reset(MeshUtils::createMeshInfo(cylinder(radiusTop, radiusBottom, height, opts)));
    mesh->initialize();
    return mesh;
}

Mesh* PrimitiveFactory::createCone(float radius, float height) {
    IConeOptions opts;
    auto* mesh = ccnew Mesh();
    mesh->reset(MeshUtils::createMeshInfo(cone(radius, height, opts)));
    mesh->initialize();
    return mesh;
}

Mesh* PrimitiveFactory::createTorus(float radius, float tube) {
    ITorusOptions opts;
    auto* mesh = ccnew Mesh();
    mesh->reset(MeshUtils::createMeshInfo(torus(radius, tube, opts)));
    mesh->initialize();
    return mesh;
}

Mesh* PrimitiveFactory::createQuad(float width, float height) {
    // quad() does not take width/height parameters directly;
    // it generates a unit quad. Width/height are unused for now.
    auto* mesh = ccnew Mesh();
    mesh->reset(MeshUtils::createMeshInfo(quad()));
    mesh->initialize();
    return mesh;
}

} // namespace cc::game
