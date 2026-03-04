#pragma once

/**
 * @file GraphicsIncludes.hpp
 * @brief Convenience header that pulls in the entire Graphics abstraction layer.
 */

// Shared cross-cutting types
#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"

// Core interfaces  (each includes its own domain-specific types & descriptors)
#include "Echelon/GraphicsAPI/Buffer.hpp"
#include "Echelon/GraphicsAPI/Texture.hpp"
#include "Echelon/GraphicsAPI/Shader.hpp"
#include "Echelon/GraphicsAPI/Pipeline.hpp"
#include "Echelon/GraphicsAPI/RenderPass.hpp"
#include "Echelon/GraphicsAPI/Framebuffer.hpp"
#include "Echelon/GraphicsAPI/CommandBuffer.hpp"
#include "Echelon/GraphicsAPI/Swapchain.hpp"
#include "Echelon/GraphicsAPI/DescriptorSet.hpp"
#include "Echelon/GraphicsAPI/Device.hpp"
#include "Echelon/GraphicsAPI/GraphicsAPI.hpp"
