/**
 * @file RayExports.cpp
 * @brief DLL export entry points for the Ray renderer plugin.
 *
 * These are the only symbols exported from Renderer.dll.
 * The engine's RendererLoader resolves them at runtime via
 * LoadLibrary / GetProcAddress (or dlopen / dlsym).
 */

#include "RayRenderer.hpp"

extern "C" {

    /**
     * @brief Factory: instantiate the Ray PBR renderer.
     * @return Heap-allocated RendererAPI*.  Caller must use DestroyRenderer().
     */
    RENDERER_EXPORT Echelon::RendererAPI* CreateRenderer() {
        return new Echelon::RayRenderer();
    }

    /**
     * @brief Destructor: free the renderer instance.
     * @param renderer Previously returned by CreateRenderer().
     */
    RENDERER_EXPORT void DestroyRenderer(Echelon::RendererAPI* renderer) {
        delete renderer;
    }

}
