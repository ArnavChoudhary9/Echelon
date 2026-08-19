#include "Asset/Mesh/Primitives.hpp"
#include "Asset/Mesh/Mesh.hpp"

#include <cmath>
#include <vector>

namespace Echelon {
namespace MeshPrimitives {

    static constexpr float kPI = 3.14159265358979323846f;

    Ref<Mesh> CreateCube() {
        // Unit cube [-0.5, 0.5] with the canonical vertex (Position/Normal/TexCoord).
        // 24 vertices (4 per face) so each face carries its own flat normal and a
        // full 0..1 UV quad. Winding is CCW when viewed from outside each face.
        std::vector<MeshVertex> vertices = {
            // +Z front  (n = 0,0,1)
            { { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
            { {  0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
            { {  0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
            { { -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
            // -Z back   (n = 0,0,-1)
            { {  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },
            { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },
            { { -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },
            { {  0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },
            // +X right  (n = 1,0,0)
            { {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
            { {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
            { {  0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
            { {  0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
            // -X left   (n = -1,0,0)
            { { -0.5f, -0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
            { { -0.5f, -0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
            { { -0.5f,  0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
            { { -0.5f,  0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
            // +Y top    (n = 0,1,0)
            { { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
            { {  0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
            { {  0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
            { { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
            // -Y bottom (n = 0,-1,0)
            { { -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
            { {  0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
            { {  0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
            { { -0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        };

        std::vector<uint32_t> indices;
        indices.reserve(36);
        for (uint32_t face = 0; face < 6; ++face) {
            const uint32_t b = face * 4;
            indices.insert(indices.end(), { b + 0, b + 1, b + 2, b + 2, b + 3, b + 0 });
        }

        auto mesh = CreateRef<Mesh>();
        mesh->SetData(std::move(vertices), std::move(indices));
        return mesh;
    }

    Ref<Mesh> CreatePlane() {
        // Unit ground quad on the XZ plane (y = 0), facing up (+Y). Scale via transform.
        std::vector<MeshVertex> vertices = {
            { { -0.5f, 0.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
            { {  0.5f, 0.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
            { {  0.5f, 0.0f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
            { { -0.5f, 0.0f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
        };
        // CCW when viewed from above (+Y), matching the up-facing normal so the top
        // survives back-face culling (front-face = CCW).
        std::vector<uint32_t> indices = { 0, 2, 1, 2, 0, 3 };

        auto mesh = CreateRef<Mesh>();
        mesh->SetData(std::move(vertices), std::move(indices));
        return mesh;
    }

    Ref<Mesh> CreateSphere(uint32_t segments) {
        // UV sphere of radius 0.5 (unit diameter) with smooth outward normals.
        if (segments < 3) segments = 3;
        const uint32_t X = segments;   // longitude
        const uint32_t Y = segments;   // latitude

        std::vector<MeshVertex> vertices;
        vertices.reserve((X + 1) * (Y + 1));
        for (uint32_t y = 0; y <= Y; ++y) {
            for (uint32_t x = 0; x <= X; ++x) {
                const float u = static_cast<float>(x) / static_cast<float>(X);
                const float v = static_cast<float>(y) / static_cast<float>(Y);
                const float px = std::cos(u * 2.0f * kPI) * std::sin(v * kPI);
                const float py = std::cos(v * kPI);
                const float pz = std::sin(u * 2.0f * kPI) * std::sin(v * kPI);
                const glm::vec3 n{ px, py, pz };
                vertices.push_back({ n * 0.5f, n, { u, v } });
            }
        }

        std::vector<uint32_t> indices;
        indices.reserve(X * Y * 6);
        for (uint32_t y = 0; y < Y; ++y) {
            for (uint32_t x = 0; x < X; ++x) {
                const uint32_t i0 = y * (X + 1) + x;
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + (X + 1);
                const uint32_t i3 = i2 + 1;
                // CCW when viewed from outside (winding matches the outward normal),
                // so front faces survive back-face culling.
                indices.insert(indices.end(), { i0, i1, i2, i1, i3, i2 });
            }
        }

        auto mesh = CreateRef<Mesh>();
        mesh->SetData(std::move(vertices), std::move(indices));
        return mesh;
    }

} // namespace MeshPrimitives
} // namespace Echelon
