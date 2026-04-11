#pragma once
#include "primitive/Primitive.h"

namespace cc::game {

class PrimitiveFactory {
public:
    static Mesh* createBox(float width = 1.0f, float height = 1.0f, float length = 1.0f);
    static Mesh* createSphere(float radius = 0.5f, uint32_t segments = 32);
    static Mesh* createPlane(float width = 10.0f, float length = 10.0f);
    static Mesh* createCylinder(float radiusTop = 0.5f, float radiusBottom = 0.5f, float height = 2.0f);
    static Mesh* createCone(float radius = 0.5f, float height = 1.0f);
    static Mesh* createTorus(float radius = 0.4f, float tube = 0.1f);
    static Mesh* createQuad(float width = 1.0f, float height = 1.0f);
};

} // namespace cc::game
