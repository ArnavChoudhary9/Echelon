#include "Asset/Importers/Texture/TextureImporter.hpp"
#include "Asset/Texture/TextureAsset.hpp"
#include "Core/Log.hpp"
#include "Instrumentation/Instrumentation.hpp"

// stb_image: the single translation unit that carries the implementation.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace Echelon {

    // ------------------------------------------------------------------
    // sRGB vs linear heuristic
    // ------------------------------------------------------------------
    // Colour textures (albedo/diffuse/emissive) are authored in sRGB and must be
    // decoded through an sRGB format so the GPU linearises them on sample. Data
    // maps (normals, roughness, metallic, AO, height, masks) hold raw values and
    // must stay linear. We cannot tell these apart from the pixels, so we key off
    // common filename conventions and default to sRGB (the common colour case).
    // A future .meta override can make this explicit per-asset.
    static bool LooksLikeLinearData(const std::string& path) {
        std::string lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        static const char* kLinearHints[] = {
            "normal", "_norm", "_nrm", "_n.", "roughness", "_rough", "_r.",
            "metallic", "metalness", "_metal", "_m.", "specular", "_spec",
            "_ao", "occlusion", "height", "_disp", "displacement", "bump",
            "mask", "_data", "_linear", "orm",
        };
        for (const char* hint : kLinearHints)
            if (lower.find(hint) != std::string::npos)
                return true;
        return false;
    }

    ImportResult TextureImporter::Import(const ImportContext& ctx) {
        ECHELON_PROFILE_FUNCTION();

        const std::string path = ctx.GetPathString();

        // OpenGL samples with the texture origin at the bottom-left, while image
        // files store the top row first. Flip on load so UVs (0,0)=top-left in the
        // authoring tool line up with the mesh texcoords.
        stbi_set_flip_vertically_on_load(1);

        int width = 0, height = 0, channels = 0;
        const bool isHDR = stbi_is_hdr(path.c_str()) != 0;

        auto texture = CreateRef<TextureAsset>();

        if (isHDR) {
            // HDR is always linear (scene-referred radiance); decode to float RGBA.
            float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
            if (!data) {
                ECHELON_LOG_ERROR("[TextureImporter] Failed to load HDR '{}': {}",
                                  path, stbi_failure_reason());
                return ImportResult(std::string("stb_image (hdr): ") + stbi_failure_reason());
            }
            const size_t byteCount = static_cast<size_t>(width) * height * 4 * sizeof(float);
            const auto* bytes = reinterpret_cast<const uint8_t*>(data);
            std::vector<uint8_t> pixels(bytes, bytes + byteCount);
            stbi_image_free(data);

            texture->SetData(std::move(pixels), static_cast<uint32_t>(width),
                             static_cast<uint32_t>(height), TextureFormat::RGBA32_FLOAT,
                             /*generateMips*/ true);

            ECHELON_LOG_INFO("[TextureImporter] Loaded '{}' ({}x{}, {} src channels, HDR/linear RGBA32F).",
                             path, width, height, channels);
            return ImportResult(texture);
        }

        // LDR: force 4 channels (RGBA8) so the GPU format always matches — simplest
        // and avoids row-alignment / format-mismatch pitfalls for odd channel counts.
        stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (!data) {
            ECHELON_LOG_ERROR("[TextureImporter] Failed to load '{}': {}",
                              path, stbi_failure_reason());
            return ImportResult(std::string("stb_image: ") + stbi_failure_reason());
        }

        const bool linear = LooksLikeLinearData(path);
        const TextureFormat format = linear ? TextureFormat::RGBA8_UNORM
                                            : TextureFormat::RGBA8_SRGB;

        const size_t byteCount = static_cast<size_t>(width) * height * 4;
        std::vector<uint8_t> pixels(data, data + byteCount);
        stbi_image_free(data);

        texture->SetData(std::move(pixels), static_cast<uint32_t>(width),
                         static_cast<uint32_t>(height), format, /*generateMips*/ true);

        ECHELON_LOG_INFO("[TextureImporter] Loaded '{}' ({}x{}, {} src channels, {}).",
                         path, width, height, channels, linear ? "linear RGBA8" : "sRGB RGBA8");
        return ImportResult(texture);
    }

} // namespace Echelon
