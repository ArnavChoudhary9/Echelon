#pragma once

/**
 * @file PlatformIncludes.hpp
 * @brief Convenience header that pulls in the entire Platform abstraction layer.
 *
 * Engine code can include this single header to get access to Window,
 * Input, and all related types.  Analogous to GraphicsIncludes.hpp.
 */

// Shared cross-cutting types
#include "Echelon/Platform/PlatformTypes.hpp"

// Core interfaces
#include "Echelon/Platform/Window.hpp"
#include "Echelon/Platform/Input.hpp"
