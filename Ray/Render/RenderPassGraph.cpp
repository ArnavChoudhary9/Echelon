#include "RenderPassGraph.hpp"

#include "GraphicsAPI/Device.hpp"
#include "GraphicsAPI/RenderPass.hpp"
#include "GraphicsAPI/Framebuffer.hpp"
#include "GraphicsAPI/CommandBuffer.hpp"
#include "GraphicsAPI/Texture.hpp"
#include "Core/Log.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Echelon {

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------

    static bool IsBackbuffer(const std::string& resource) {
        return resource == kBackbufferResource;
    }

    // Format of a declared resource (fallback when the name is unknown / $backbuffer).
    static TextureFormat ResourceFormat(const RenderPipelineDesc& desc,
                                        const std::string& resource,
                                        TextureFormat fallback) {
        for (const auto& r : desc.Resources)
            if (r.Name == resource) return r.Format;
        return fallback;
    }

    // The resource a pass sizes itself from: first color output, else depth output.
    static const std::string* PrimaryOutputResource(const PassDesc& pass) {
        if (!pass.ColorOutputs.empty())
            return &pass.ColorOutputs.front().Resource;
        if (pass.DepthOutput)
            return &pass.DepthOutput->Resource;
        return nullptr;
    }

    void RenderPassGraph::ResolveResourceSize(const std::string& resource,
                                              uint32_t& outW, uint32_t& outH) const {
        for (const auto& r : m_Desc.Resources) {
            if (r.Name != resource) continue;
            if (r.SizePolicy == ResourceSizePolicy::Fixed) {
                outW = std::max(1u, r.Width);
                outH = std::max(1u, r.Height);
            } else {
                outW = static_cast<uint32_t>(std::max(1.0f, std::round(m_Width  * r.Scale)));
                outH = static_cast<uint32_t>(std::max(1.0f, std::round(m_Height * r.Scale)));
            }
            return;
        }
        // Unknown / $backbuffer -> swapchain size.
        outW = m_Width;
        outH = m_Height;
    }

    void RenderPassGraph::ComputePassSize(const PassDesc& pass,
                                          uint32_t& outW, uint32_t& outH) const {
        const std::string* primary = PrimaryOutputResource(pass);
        if (!primary || IsBackbuffer(*primary)) {
            outW = m_Width;
            outH = m_Height;
            return;
        }
        ResolveResourceSize(*primary, outW, outH);
    }

    // ------------------------------------------------------------------
    // Compile
    // ------------------------------------------------------------------

    bool RenderPassGraph::CompileFrom(const RenderPipelineDesc& desc,
                                      const Ref<Device>& device,
                                      uint32_t width, uint32_t height) {
        m_Device = device;
        m_Desc   = desc;
        m_Width  = width;
        m_Height = height;
        m_Order.clear();
        m_PassIndexByName.clear();
        m_ResourceProducers.clear();
        m_ComputeResources.clear();

        if (!device) {
            ECHELON_LOG_ERROR("RenderPassGraph: no device");
            return false;
        }

        // ---- Enabled passes only ----
        std::vector<const PassDesc*> passes;
        for (const auto& p : desc.Passes)
            if (p.Enabled) passes.push_back(&p);

        if (passes.empty()) {
            ECHELON_LOG_ERROR("RenderPassGraph: description has no enabled passes");
            return false;
        }

        // ---- Declared resource names (for validation) ----
        std::unordered_set<std::string> declared;
        for (const auto& r : desc.Resources) {
            if (!declared.insert(r.Name).second)
                ECHELON_LOG_WARN("RenderPassGraph: duplicate resource '{}'", r.Name);
        }

        // ---- Producer map: resource -> index into `passes` ----
        std::unordered_map<std::string, size_t> producerOf;
        for (size_t i = 0; i < passes.size(); ++i) {
            const PassDesc& p = *passes[i];
            auto record = [&](const std::string& res) -> bool {
                if (IsBackbuffer(res)) return true;          // external sink, no producer edge
                if (!declared.count(res)) {
                    ECHELON_LOG_ERROR("RenderPassGraph: pass '{}' writes undeclared resource '{}'", p.Name, res);
                    return false;
                }
                if (!producerOf.emplace(res, i).second) {
                    ECHELON_LOG_ERROR("RenderPassGraph: resource '{}' written by more than one pass", res);
                    return false;
                }
                return true;
            };
            for (const auto& c : p.ColorOutputs) if (!record(c.Resource)) return false;
            if (p.DepthOutput)                    if (!record(p.DepthOutput->Resource)) return false;
            // A compute pass "produces" the storage images it writes (its AsStorageImage
            // inputs), so a later pass that samples them gets a real dependency edge.
            if (p.Type == PassType::Compute)
                for (const auto& in : p.Inputs)
                    if (in.AsStorageImage && !record(in.Resource)) return false;
        }

        // ---- Edges + in-degrees (P depends on producer of each input) ----
        std::vector<std::vector<size_t>> adj(passes.size());
        std::vector<int> indeg(passes.size(), 0);
        for (size_t i = 0; i < passes.size(); ++i) {
            for (const auto& in : passes[i]->Inputs) {
                // A compute pass's storage-image input is an OUTPUT it writes, not a read —
                // it must not create a dependency on its own producer (or itself).
                if (passes[i]->Type == PassType::Compute && in.AsStorageImage) continue;
                if (IsBackbuffer(in.Resource)) {
                    ECHELON_LOG_ERROR("RenderPassGraph: pass '{}' cannot sample the backbuffer", passes[i]->Name);
                    return false;
                }
                auto it = producerOf.find(in.Resource);
                if (it == producerOf.end()) {
                    ECHELON_LOG_ERROR("RenderPassGraph: pass '{}' reads resource '{}' that no pass writes",
                                      passes[i]->Name, in.Resource);
                    return false;
                }
                adj[it->second].push_back(i);
                ++indeg[i];
            }
        }

        // ---- Kahn's algorithm; ties broken by declaration order (smallest index) ----
        std::vector<size_t> order;
        order.reserve(passes.size());
        std::vector<bool> emitted(passes.size(), false);
        while (order.size() < passes.size()) {
            size_t pick = passes.size();
            for (size_t i = 0; i < passes.size(); ++i) {
                if (!emitted[i] && indeg[i] == 0) { pick = i; break; }
            }
            if (pick == passes.size()) {
                ECHELON_LOG_ERROR("RenderPassGraph: cycle in pass dependencies");
                m_Order.clear();
                return false;
            }
            emitted[pick] = true;
            order.push_back(pick);
            for (size_t next : adj[pick]) --indeg[next];
        }

        // ---- Materialise compiled passes in resolved order ----
        m_Order.reserve(order.size());
        for (size_t idx : order) {
            const PassDesc& p = *passes[idx];
            CompiledPass cp;
            cp.Desc = p;
            cp.Backbuffer = std::any_of(p.ColorOutputs.begin(), p.ColorOutputs.end(),
                                        [](const AttachmentRef& a){ return IsBackbuffer(a.Resource); });
            ComputePassSize(p, cp.Width, cp.Height);

            // Create the GPU RenderPass (Graphics / Fullscreen only).
            if (p.Type != PassType::Compute) {
                RenderPassDesc rp;
                for (const auto& c : p.ColorOutputs) {
                    ColorAttachmentDesc ca;
                    ca.Format = ResourceFormat(desc, c.Resource, TextureFormat::RGBA8_UNORM);
                    ca.Load  = c.Load;
                    ca.Store = c.Store;
                    ca.Clear = c.ColorClear;
                    rp.ColorAttachments.push_back(ca);
                }
                if (p.DepthOutput) {
                    DepthAttachmentDesc da;
                    da.Format = ResourceFormat(desc, p.DepthOutput->Resource, TextureFormat::D32_FLOAT);
                    da.Load  = p.DepthOutput->Load;
                    da.Store = p.DepthOutput->Store;
                    da.Clear = p.DepthOutput->DepthClear;
                    rp.DepthAttachment    = da;
                    rp.HasDepthAttachment = true;
                }
                rp.DebugName = "PassGraph_" + p.Name;
                cp.Pass = device->CreateRenderPass(rp);
            }

            m_Order.push_back(std::move(cp));
        }

        // ---- Name index + resource producer locations (into m_Order) ----
        for (size_t i = 0; i < m_Order.size(); ++i) {
            const CompiledPass& cp = m_Order[i];
            m_PassIndexByName[cp.Desc.Name] = i;
            for (uint32_t c = 0; c < cp.Desc.ColorOutputs.size(); ++c) {
                const std::string& res = cp.Desc.ColorOutputs[c].Resource;
                if (!IsBackbuffer(res))
                    m_ResourceProducers[res] = AttachmentLocation{ i, false, c };
            }
            if (cp.Desc.DepthOutput)
                m_ResourceProducers[cp.Desc.DepthOutput->Resource] = AttachmentLocation{ i, true, 0 };
            if (cp.Desc.Type == PassType::Compute)
                for (const auto& in : cp.Desc.Inputs)
                    if (in.AsStorageImage)
                        m_ResourceProducers[in.Resource] = AttachmentLocation{ i, false, 0 };
        }

        // ---- Resolve each pass's inputs to producing attachment locations ----
        for (auto& cp : m_Order) {
            cp.Inputs.clear();
            cp.Inputs.reserve(cp.Desc.Inputs.size());
            for (const auto& in : cp.Desc.Inputs) {
                auto it = m_ResourceProducers.find(in.Resource);
                cp.Inputs.push_back(it != m_ResourceProducers.end() ? it->second : AttachmentLocation{});
            }
        }

        if (!CreateFramebuffers()) {
            m_Order.clear();
            m_PassIndexByName.clear();
            m_ResourceProducers.clear();
            m_ComputeResources.clear();
            return false;
        }
        CreateComputeResources();
        BuildOffscreenTarget();   // (re)build the editor viewport target against the new passes

        ECHELON_LOG_INFO("RenderPassGraph: compiled {} pass(es) at {}x{}", m_Order.size(), m_Width, m_Height);
        for (const auto& cp : m_Order)
            ECHELON_LOG_DEBUG("  pass '{}' type={} backbuffer={} hasFB={} {}x{}",
                              cp.Desc.Name, static_cast<int>(cp.Desc.Type),
                              cp.Backbuffer, static_cast<bool>(cp.FB), cp.Width, cp.Height);
        return true;
    }

    // ------------------------------------------------------------------
    // Framebuffers
    // ------------------------------------------------------------------

    bool RenderPassGraph::CreateFramebuffers() {
        for (auto& cp : m_Order) {
            ComputePassSize(cp.Desc, cp.Width, cp.Height);

            // Compute passes and backbuffer passes have no owned framebuffer.
            if (cp.Desc.Type == PassType::Compute || cp.Backbuffer) {
                cp.FB = nullptr;
                continue;
            }

            FramebufferDesc fb;
            fb.Width  = cp.Width;
            fb.Height = cp.Height;
            for (const auto& c : cp.Desc.ColorOutputs) {
                FramebufferAttachment att;
                att.ExistingTexture = nullptr;   // auto-create (sampleable)
                for (const auto& r : m_Desc.Resources)
                    if (r.Name == c.Resource) { att.Format = r.Format; break; }
                fb.ColorAttachments.push_back(att);
            }
            if (cp.Desc.DepthOutput) {
                FramebufferAttachment depth;
                depth.ExistingTexture = nullptr;
                for (const auto& r : m_Desc.Resources)
                    if (r.Name == cp.Desc.DepthOutput->Resource) { depth.Format = r.Format; break; }
                fb.DepthAttachment    = depth;
                fb.HasDepthAttachment = true;
            }
            fb.CompatiblePass = cp.Pass;
            fb.Samples        = cp.Desc.Samples;   // >1 → MSAA render target, auto-resolved
            fb.DebugName      = "PassGraphFB_" + cp.Desc.Name;

            cp.FB = m_Device->CreateFramebuffer(fb);
            if (!cp.FB) {
                ECHELON_LOG_ERROR("RenderPassGraph: failed to create framebuffer for pass '{}'", cp.Desc.Name);
                return false;
            }
        }
        return true;
    }

    // Standalone storage textures for resources a compute pass writes (and later passes
    // sample). Unlike color/depth attachments these aren't owned by any framebuffer.
    void RenderPassGraph::CreateComputeResources() {
        m_ComputeResources.clear();
        if (!m_Device) return;
        for (const auto& cp : m_Order) {
            if (cp.Desc.Type != PassType::Compute) continue;
            for (const auto& in : cp.Desc.Inputs) {
                if (!in.AsStorageImage || IsBackbuffer(in.Resource)) continue;
                if (m_ComputeResources.count(in.Resource)) continue;

                uint32_t w = m_Width, h = m_Height;
                ResolveResourceSize(in.Resource, w, h);
                TextureDesc td;
                td.Type      = TextureType::StorageTexture;
                td.Format    = ResourceFormat(m_Desc, in.Resource, TextureFormat::RGBA16_FLOAT);
                td.Usage     = TextureUsage::Storage | TextureUsage::Sampled;
                td.Width     = w;
                td.Height    = h;
                td.DebugName = "PassGraphCompute_" + in.Resource;
                m_ComputeResources[in.Resource] = m_Device->CreateTexture(td);
            }
        }
    }

    void RenderPassGraph::Resize(uint32_t width, uint32_t height) {
        if (width == m_Width && height == m_Height) return;
        m_Width  = width;
        m_Height = height;
        if (!m_Order.empty()) {
            if (!CreateFramebuffers())
                ECHELON_LOG_ERROR("[RenderPassGraph] Framebuffer recreation failed on resize to {}x{}.",
                                  width, height);
            CreateComputeResources();
            BuildOffscreenTarget();   // editor viewport target follows the graph size
        }
    }

    // ------------------------------------------------------------------
    // Callbacks & execution
    // ------------------------------------------------------------------

    void RenderPassGraph::SetExecuteCallback(const std::string& passName, ExecuteFn fn) {
        m_Callbacks[passName] = std::move(fn);
    }

    void RenderPassGraph::SetDefaultCallback(PassType type, ExecuteFn fn) {
        m_DefaultCallbacks[static_cast<int>(type)] = std::move(fn);
    }

    void RenderPassGraph::Execute(const Ref<CommandBuffer>& cmd) {
        if (!cmd) return;

        for (const auto& cp : m_Order) {
            // Backbuffer passes normally target the window default framebuffer (null).
            // When offscreen redirection is on (editor viewport), send them to the
            // offscreen FBO instead so the window is free for the UI.
            Ref<Framebuffer> target = cp.FB;
            if (cp.Backbuffer)
                target = m_OffscreenEnabled ? m_OffscreenFB : nullptr;

            PassContext ctx;
            ctx.Pass   = &cp.Desc;
            ctx.Target = target;
            ctx.Width  = cp.Width;
            ctx.Height = cp.Height;

            ctx.InputTextures.reserve(cp.Inputs.size());
            for (size_t k = 0; k < cp.Inputs.size(); ++k) {
                const std::string& resName = cp.Desc.Inputs[k].Resource;
                Ref<Texture> tex;
                if (auto cit = m_ComputeResources.find(resName); cit != m_ComputeResources.end()) {
                    tex = cit->second;   // standalone storage texture written by a compute pass
                } else {
                    const auto& loc = cp.Inputs[k];
                    const auto& producer = m_Order[loc.PassIndex];
                    tex = producer.FB
                        ? (loc.IsDepth ? producer.FB->GetDepthAttachment()
                                       : producer.FB->GetColorAttachment(loc.ColorIndex))
                        : nullptr;
                }
                ctx.InputTextures.push_back(tex);
            }

            // Resolve the pass's work: name-specific callback first, else the
            // per-type default handler (e.g. a generic fullscreen/compute handler).
            const ExecuteFn* cb = nullptr;
            if (auto cbIt = m_Callbacks.find(cp.Desc.Name); cbIt != m_Callbacks.end() && cbIt->second) {
                cb = &cbIt->second;
            } else if (const ExecuteFn& def = m_DefaultCallbacks[static_cast<int>(cp.Desc.Type)]; def) {
                cb = &def;
            }

            if (cp.Desc.Type == PassType::Compute) {
                // No render-pass scope for compute (barriers added in a later phase).
                if (cb) (*cb)(*cmd, ctx);
                continue;
            }

            Viewport vp;
            vp.Width  = static_cast<float>(cp.Width);
            vp.Height = static_cast<float>(cp.Height);
            cmd->SetViewport(vp);
            cmd->BeginRenderPass(cp.Pass, target);
            if (cb) (*cb)(*cmd, ctx);
            cmd->EndRenderPass();
        }
    }

    // ------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------

    Ref<RenderPass> RenderPassGraph::GetRenderPass(const std::string& passName) const {
        auto it = m_PassIndexByName.find(passName);
        if (it == m_PassIndexByName.end()) return nullptr;
        return m_Order[it->second].Pass;
    }

    Ref<Texture> RenderPassGraph::GetOutput(const std::string& resourceName) const {
        if (auto cit = m_ComputeResources.find(resourceName); cit != m_ComputeResources.end())
            return cit->second;
        auto it = m_ResourceProducers.find(resourceName);
        if (it == m_ResourceProducers.end()) return nullptr;
        const auto& loc = it->second;
        const auto& producer = m_Order[loc.PassIndex];
        if (!producer.FB) return nullptr;
        return loc.IsDepth ? producer.FB->GetDepthAttachment()
                           : producer.FB->GetColorAttachment(loc.ColorIndex);
    }

    bool RenderPassGraph::ReadResourcePixel(const std::string& resourceName, uint32_t x, uint32_t y,
                                            void* out, uint32_t outSize) const {
        auto it = m_ResourceProducers.find(resourceName);
        if (it == m_ResourceProducers.end()) return false;
        const auto& loc = it->second;
        if (loc.IsDepth) return false;                       // depth readback unsupported
        if (loc.PassIndex >= m_Order.size()) return false;
        const auto& producer = m_Order[loc.PassIndex];
        if (!producer.FB) return false;                      // backbuffer / compute — no FB to read
        return producer.FB->ReadPixel(loc.ColorIndex, static_cast<int32_t>(x), static_cast<int32_t>(y),
                                      out, outSize);
    }

    // ------------------------------------------------------------------
    // Offscreen backbuffer redirection (editor viewport)
    // ------------------------------------------------------------------

    void RenderPassGraph::SetOffscreenTarget(bool enabled) {
        if (m_OffscreenEnabled == enabled) return;
        m_OffscreenEnabled = enabled;
        if (enabled)
            BuildOffscreenTarget();
        else
            m_OffscreenFB = nullptr;
    }

    Ref<Texture> RenderPassGraph::GetOffscreenColor() const {
        return m_OffscreenFB ? m_OffscreenFB->GetColorAttachment(0) : nullptr;
    }

    // (Re)create an offscreen framebuffer that mirrors the backbuffer pass's
    // attachment layout, so a pass writing $backbuffer can draw into a sampleable
    // texture instead of the window. Rebuilt whenever the passes or size change.
    void RenderPassGraph::BuildOffscreenTarget() {
        m_OffscreenFB = nullptr;
        if (!m_OffscreenEnabled || !m_Device) return;
        if (m_Width == 0 || m_Height == 0) return;

        // First pass that writes $backbuffer is the one we redirect.
        const CompiledPass* bb = nullptr;
        for (const auto& cp : m_Order)
            if (cp.Backbuffer) { bb = &cp; break; }
        if (!bb || !bb->Pass) return;   // no backbuffer pass → nothing to redirect

        FramebufferDesc fb;
        fb.Width  = m_Width;
        fb.Height = m_Height;
        for (const auto& c : bb->Desc.ColorOutputs) {
            FramebufferAttachment att;
            att.ExistingTexture = nullptr;   // auto-create a sampleable color texture
            att.Format = ResourceFormat(m_Desc, c.Resource, TextureFormat::RGBA8_UNORM);
            fb.ColorAttachments.push_back(att);
        }
        if (bb->Desc.DepthOutput) {
            FramebufferAttachment depth;
            depth.ExistingTexture = nullptr;
            depth.Format = ResourceFormat(m_Desc, bb->Desc.DepthOutput->Resource, TextureFormat::D32_FLOAT);
            fb.DepthAttachment    = depth;
            fb.HasDepthAttachment = true;
        }
        fb.CompatiblePass = bb->Pass;
        fb.Samples        = bb->Desc.Samples;   // resolved on EndRenderPass if >1
        fb.DebugName      = "PassGraph_OffscreenViewport";

        m_OffscreenFB = m_Device->CreateFramebuffer(fb);
        if (!m_OffscreenFB)
            ECHELON_LOG_ERROR("RenderPassGraph: failed to create offscreen viewport target ({}x{})",
                              m_Width, m_Height);
    }

} // namespace Echelon
