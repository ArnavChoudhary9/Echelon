#include "Asset/Mesh/Primitives.hpp"
#include "Asset/Mesh/Mesh.hpp"

namespace Echelon {
namespace MeshPrimitives {

    Ref<Mesh> CreateCube() {
        // 8 unique corners: position (vec3) + color (vec3), matching the Flat pipeline layout.
        std::vector<MeshVertex> vertices = {
            { { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.3f, 0.3f } }, // 0 back-bottom-left
            { {  0.5f, -0.5f, -0.5f }, { 0.3f, 1.0f, 0.3f } }, // 1 back-bottom-right
            { {  0.5f,  0.5f, -0.5f }, { 0.3f, 0.3f, 1.0f } }, // 2 back-top-right
            { { -0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 0.3f } }, // 3 back-top-left
            { { -0.5f, -0.5f,  0.5f }, { 1.0f, 0.3f, 1.0f } }, // 4 front-bottom-left
            { {  0.5f, -0.5f,  0.5f }, { 0.3f, 1.0f, 1.0f } }, // 5 front-bottom-right
            { {  0.5f,  0.5f,  0.5f }, { 1.0f, 0.7f, 0.3f } }, // 6 front-top-right
            { { -0.5f,  0.5f,  0.5f }, { 0.3f, 0.7f, 1.0f } }, // 7 front-top-left
        };

        std::vector<uint32_t> indices = {
            0, 1, 2,  2, 3, 0,  // back
            4, 6, 5,  6, 4, 7,  // front
            0, 3, 7,  7, 4, 0,  // left
            1, 5, 6,  6, 2, 1,  // right
            0, 4, 5,  5, 1, 0,  // bottom
            3, 2, 6,  6, 7, 3,  // top
        };

        auto mesh = CreateRef<Mesh>();
        mesh->SetData(std::move(vertices), std::move(indices));
        return mesh;
    }

} // namespace MeshPrimitives
} // namespace Echelon
