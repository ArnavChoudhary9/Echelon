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
        }

        // ---- Edges + in-degrees (P depends on producer of each input) ----
        std::vector<std::vector<size_t>> adj(passes.size());
        std::vector<int> indeg(passes.size(), 0);
        for (size_t i = 0; i < passes.size(); ++i) {
            for (const auto& in : passes[i]->Inputs) {
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
            return false;
        }

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
            fb.DebugName      = "PassGraphFB_" + cp.Desc.Name;

            cp.FB = m_Device->CreateFramebuffer(fb);
            if (!cp.FB) {
                ECHELON_LOG_ERROR("RenderPassGraph: failed to create framebuffer for pass '{}'", cp.Desc.Name);
                return false;
            }
        }
        return true;
    }

    void RenderPassGraph::Resize(uint32_t width, uint32_t height) {
        if (width == m_Width && height == m_Height) return;
        m_Width  = width;
        m_Height = height;
        if (!m_Order.empty())
            CreateFramebuffers();
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
            PassContext ctx;
            ctx.Pass   = &cp.Desc;
            ctx.Target = cp.FB;
            ctx.Width  = cp.Width;
            ctx.Height = cp.Height;

            ctx.InputTextures.reserve(cp.Inputs.size());
            for (const auto& loc : cp.Inputs) {
                const auto& producer = m_Order[loc.PassIndex];
                Ref<Texture> tex = producer.FB
                    ? (loc.IsDepth ? producer.FB->GetDepthAttachment()
                                   : producer.FB->GetColorAttachment(loc.ColorIndex))
                    : nullptr;
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
            cmd->BeginRenderPass(cp.Pass, cp.FB);
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
        auto it = m_ResourceProducers.find(resourceName);
        if (it == m_ResourceProducers.end()) return nullptr;
        const auto& loc = it->second;
        const auto& producer = m_Order[loc.PassIndex];
        if (!producer.FB) return nullptr;
        return loc.IsDepth ? producer.FB->GetDepthAttachment()
                           : producer.FB->GetColorAttachment(loc.ColorIndex);
    }

} // namespace Echelon
