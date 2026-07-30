#pragma once

/**
 * @file Mesh.hpp
 * @brief Renderable mesh asset — owns CPU geometry and the GPU buffers built from it.
 */

#include "GraphicsAPI/Buffer.hpp"
#include "GraphicsAPI/Device.hpp"

#include "Renderer/RendererAPI.hpp"

#include "Asset/Asset.hpp"
#include "Asset/Mesh/MeshVertex.hpp"

#include <vector>
#include <cstdint>

namespace Echelon {

/**
 * @brief A mesh asset: interleaved CPU vertices/indices plus the GPU buffers
 *        realized from them.
 *
 * The CPU arrays are retained so the GPU buffers can be rebuilt on demand — this
 * is what makes renderer/back-end hot-swap and asset hot-reload possible
 * (BufferDesc::InitialData is non-owning, so the geometry must live here).
 * Uploads go through the active renderer's Device, so there is no back-end
 * specific code in this class.
 */
class Mesh : public Asset {
public:
    Mesh() = default;
    ~Mesh() override = default;

    /** @brief Replace the CPU-side geometry and recompute buffer descriptors. Does not touch the GPU. */
    void SetData(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices) {
        m_Vertices = std::move(vertices);
        m_Indices  = std::move(indices);

        m_VertexCount = static_cast<uint32_t>(m_Vertices.size());
        m_IndexCount  = static_cast<uint32_t>(m_Indices.size());

        m_VertexBufferDesc = {};
        m_VertexBufferDesc.Size      = m_Vertices.size() * sizeof(MeshVertex);
        m_VertexBufferDesc.Usage     = BufferUsage::VertexBuffer;
        m_VertexBufferDesc.Memory    = MemoryUsage::GPUOnly;
        m_VertexBufferDesc.DebugName = "MeshVB";

        m_IndexBufferDesc = {};
        m_IndexBufferDesc.Size      = m_Indices.size() * sizeof(uint32_t);
        m_IndexBufferDesc.Usage     = BufferUsage::IndexBuffer;
        m_IndexBufferDesc.Memory    = MemoryUsage::GPUOnly;
        m_IndexBufferDesc.DebugName = "MeshIB";
    }

    AssetType GetType() const override { return AssetType::Mesh; }

    /** @brief (Re)create the GPU buffers from the retained CPU geometry. */
    void UploadGPU(RendererAPI* renderer) override {
        if (!renderer) return;
        auto device = renderer->GetDevice();
        if (!device) return;

        if (!m_VertexBuffer && !m_Vertices.empty()) {
            m_VertexBufferDesc.InitialData = m_Vertices.data();
            m_VertexBuffer = device->CreateBuffer(m_VertexBufferDesc);
        }
        if (!m_IndexBuffer && !m_Indices.empty()) {
            m_IndexBufferDesc.InitialData = m_Indices.data();
            m_IndexBuffer = device->CreateBuffer(m_IndexBufferDesc);
        }
    }

    /** @brief Drop the GPU buffers; CPU geometry is retained for a later rebuild. */
    void ReleaseGPU() override {
        m_VertexBuffer = nullptr;
        m_IndexBuffer  = nullptr;
    }

    /** @brief Absorb re-imported geometry in place so held Refs stay valid (hot-reload). */
    void ReloadFrom(const Ref<Asset>& fresh) override {
        auto other = std::dynamic_pointer_cast<Mesh>(fresh);
        if (!other) return;
        ReleaseGPU();
        SetData(other->m_Vertices, other->m_Indices);
    }

    [[nodiscard]] bool IsValid() const override {
        return m_VertexBuffer != nullptr && m_VertexCount > 0;
    }

    [[nodiscard]] Ref<Buffer> GetVertexBuffer() const { return m_VertexBuffer; }
    [[nodiscard]] Ref<Buffer> GetIndexBuffer()  const { return m_IndexBuffer; }
    uint32_t GetVertexCount() const { return m_VertexCount; }
    uint32_t GetIndexCount()  const { return m_IndexCount; }
    const std::vector<MeshVertex>& GetVertices() const { return m_Vertices; }
    const std::vector<uint32_t>&   GetIndices()  const { return m_Indices; }
    const BufferDesc& GetVertexBufferDesc() const { return m_VertexBufferDesc; }
    const BufferDesc& GetIndexBufferDesc()  const { return m_IndexBufferDesc; }

private:
    std::vector<MeshVertex> m_Vertices;
    std::vector<uint32_t>   m_Indices;

    Ref<Buffer> m_VertexBuffer = nullptr;
    Ref<Buffer> m_IndexBuffer  = nullptr;
    uint32_t    m_VertexCount  = 0;
    uint32_t    m_IndexCount   = 0;
    BufferDesc  m_VertexBufferDesc;
    BufferDesc  m_IndexBufferDesc;
};

} // namespace Echelon
