#pragma once

/**
 * @file StandardVertex.hpp
 * @brief The one canonical vertex definition + reflection→VertexLayout mapper.
 *
 * This is what removes all per-shader special-casing: any shader that declares
 * vertex inputs at the canonical locations (Position=0, Normal=1, TexCoord=2) gets
 * a correct VertexLayout mapped onto the single MeshVertex, so the mesh loader and
 * pipeline never hand-sync attribute layouts.
 */

#include "Asset/Mesh/MeshVertex.hpp"
#include "GraphicsAPI/Pipeline.hpp"          // VertexLayout, VertexAttribute*
#include "GraphicsAPI/ShaderReflection.hpp"  // ShaderReflection

#include <cstddef>
#include <vector>

namespace Echelon {
namespace StandardVertex {

    /** @brief One canonical attribute: shader location → (name, format, byte offset in MeshVertex). */
    struct Entry {
        uint32_t              Location;
        const char*           Name;
        VertexAttributeFormat Format;
        uint32_t              Offset;
    };

    /** @brief Total interleaved stride of the canonical vertex. */
    inline constexpr uint32_t Stride = static_cast<uint32_t>(sizeof(MeshVertex));

    /** @brief The canonical attribute table, keyed by shader input location. */
    inline const std::vector<Entry>& Table() {
        static const std::vector<Entry> table = {
            { 0, "Position", VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(MeshVertex, Position)) },
            { 1, "Normal",   VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(MeshVertex, Normal))   },
            { 2, "TexCoord", VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(MeshVertex, TexCoord)) },
        };
        return table;
    }

    /** @brief Look up the canonical entry for a shader input location, or nullptr. */
    inline const Entry* Find(uint32_t location) {
        for (const auto& e : Table())
            if (e.Location == location) return &e;
        return nullptr;
    }

    /**
     * @brief Build a VertexLayout for a shader from its reflected vertex inputs.
     *
     * One binding (0, stride = sizeof(MeshVertex), per-vertex). Each reflected
     * input is mapped to its canonical entry by location, so only the attributes
     * the shader actually consumes are declared — the interleaved offsets always
     * come from the canonical vertex.
     */
    inline VertexLayout FromReflection(const ShaderReflection& refl) {
        VertexLayout layout;

        VertexBinding binding;
        binding.Binding   = 0;
        binding.Stride    = Stride;
        binding.InputRate = VertexInputRate::PerVertex;
        layout.Bindings.push_back(binding);

        for (const auto& in : refl.VertexInputs) {
            const Entry* e = Find(in.Location);
            if (!e) continue;  // shader input with no canonical slot — ignore.

            VertexAttribute attr;
            attr.Name     = e->Name;
            attr.Format   = e->Format;   // canonical format for this slot
            attr.Offset   = e->Offset;
            attr.Binding  = 0;
            attr.Location = in.Location;
            layout.Attributes.push_back(attr);
        }
        return layout;
    }

} // namespace StandardVertex
} // namespace Echelon
