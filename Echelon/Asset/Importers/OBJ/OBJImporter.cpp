#include "Asset/Importers/OBJ/OBJImporter.hpp"
#include "Asset/Mesh/Mesh.hpp"
#include "Core/Log.hpp"

#include "glm/glm.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <exception>

namespace Echelon {

    ImportResult OBJImporter::Import(const ImportContext& ctx) {
        const std::string path = ctx.GetPathString();

        // tinyobj resolves the .mtl sidecar relative to this directory; without it
        // an OBJ with a mtllib emits a spurious "material not found" warning.
        std::string baseDir = ctx.GetPath().parent_path().string();
        if (!baseDir.empty()) baseDir += "/";

        tinyobj::attrib_t                attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;
        std::string                      warn;
        std::string                      err;

        bool ok = false;
        try {
            // triangulate = true so quads/ngons become triangles.
            ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                                  path.c_str(), baseDir.empty() ? nullptr : baseDir.c_str());
        } catch (const std::exception& e) {
            ECHELON_LOG_ERROR("[OBJImporter] Exception loading '{}': {}", path, e.what());
            return ImportResult(std::string("Exception: ") + e.what());
        }

        if (!warn.empty())
            ECHELON_LOG_WARN("[OBJImporter] '{}': {}", path, warn);

        if (!ok) {
            ECHELON_LOG_ERROR("[OBJImporter] Failed to load '{}': {}", path,
                              err.empty() ? "unknown error" : err);
            return ImportResult(err.empty() ? "Failed to load OBJ" : err);
        }

        std::vector<MeshVertex> vertices;
        std::vector<uint32_t>   indices;
        vertices.reserve(attrib.vertices.size() / 3);

        const size_t vertexFloats   = attrib.vertices.size();
        const size_t normalFloats   = attrib.normals.size();
        const size_t texcoordFloats = attrib.texcoords.size();

        // Emit the single canonical vertex (Position/Normal/TexCoord) straight from the
        // OBJ. No shader-specific special-casing — the pipeline maps its declared inputs
        // onto this via reflection (see StandardVertex::FromReflection).
        for (const auto& shape : shapes) {
            for (const auto& idx : shape.mesh.indices) {
                if (idx.vertex_index < 0) continue; // malformed face — skip defensively

                const size_t vbase = static_cast<size_t>(idx.vertex_index) * 3;
                if (vbase + 2 >= vertexFloats) continue; // out of range — skip, never crash

                MeshVertex v;
                v.Position = {
                    attrib.vertices[vbase + 0],
                    attrib.vertices[vbase + 1],
                    attrib.vertices[vbase + 2]
                };

                if (idx.normal_index >= 0 &&
                    static_cast<size_t>(idx.normal_index) * 3 + 2 < normalFloats) {
                    const size_t nbase = static_cast<size_t>(idx.normal_index) * 3;
                    v.Normal = { attrib.normals[nbase + 0],
                                 attrib.normals[nbase + 1],
                                 attrib.normals[nbase + 2] };
                } else {
                    // No normal in the file — a neutral default (lighting shaders can
                    // recompute face normals later if needed).
                    v.Normal = { 0.0f, 0.0f, 1.0f };
                }

                if (idx.texcoord_index >= 0 &&
                    static_cast<size_t>(idx.texcoord_index) * 2 + 1 < texcoordFloats) {
                    const size_t tbase = static_cast<size_t>(idx.texcoord_index) * 2;
                    v.TexCoord = { attrib.texcoords[tbase + 0],
                                   attrib.texcoords[tbase + 1] };
                } else {
                    v.TexCoord = { 0.0f, 0.0f };
                }

                indices.push_back(static_cast<uint32_t>(vertices.size()));
                vertices.push_back(v);
            }
        }

        if (vertices.empty()) {
            ECHELON_LOG_ERROR("[OBJImporter] '{}' contained no usable geometry.", path);
            return ImportResult("OBJ contained no geometry");
        }

        auto mesh = CreateRef<Mesh>();
        mesh->SetData(std::move(vertices), std::move(indices));

        ECHELON_LOG_INFO("[OBJImporter] Loaded '{}' ({} vertices, {} indices).",
                         path, mesh->GetVertexCount(), mesh->GetIndexCount());
        return ImportResult(mesh);
    }

} // namespace Echelon
