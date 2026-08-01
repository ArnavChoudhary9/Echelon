#pragma once

// Logging is active in Debug and Release builds.
// In Dist builds (ECHELON_DIST) all macros become no-ops so the logger and
// its spdlog dependency are compiled out entirely — zero overhead in shipped games.
#ifndef ECHELON_DIST

    #include "Echelon/Logger/Logger.hpp"
    #include "Echelon/Core/Base.hpp"

    namespace Echelon {
        // Inline variable: one shared instance across all TUs and shared libs.
        inline Ref<Logger> s_CoreLogger;
    }

    #define INIT_ECHELON_LOGGER() \
        ::Echelon::s_CoreLogger = ::Echelon::CreateRef<::Echelon::Logger>("ECHELON"); \
        ::Echelon::s_CoreLogger->AddSink(::Echelon::ConsoleSink); \
        ::Echelon::s_CoreLogger->AddSink(::Echelon::FileSink("ECHELON.log"));

    // Release the core logger deterministically before the program exits.
    // s_CoreLogger is a global; if it were allowed to destruct during static
    // teardown it would call spdlog::drop() after spdlog's own registry singleton
    // is already gone (static-init-order fiasco) — an intermittent crash on exit.
    // Resetting it here, while the registry is still alive, makes teardown safe.
    #define SHUTDOWN_ECHELON_LOGGER() ::Echelon::s_CoreLogger.reset();

    #define ECHELON_LOG_TRACE(...) do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Trace(__VA_ARGS__); } while(0)
    #define ECHELON_LOG_INFO(...)  do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Info(__VA_ARGS__); } while(0)
    #define ECHELON_LOG_DEBUG(...) do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Debug(__VA_ARGS__); } while(0)
    #define ECHELON_LOG_WARN(...)  do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Warn(__VA_ARGS__); } while(0)
    #define ECHELON_LOG_ERROR(...) do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Error(__VA_ARGS__); } while(0)
    #define ECHELON_LOG_FATAL(...) do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Fatal(__VA_ARGS__); } while(0)

#else // ECHELON_DIST — all logging compiled out

    #define INIT_ECHELON_LOGGER()
    #define SHUTDOWN_ECHELON_LOGGER()
    #define ECHELON_LOG_TRACE(...)
    #define ECHELON_LOG_INFO(...)
    #define ECHELON_LOG_DEBUG(...)
    #define ECHELON_LOG_WARN(...)
    #define ECHELON_LOG_ERROR(...)
    #define ECHELON_LOG_FATAL(...)

#endif // ECHELON_DIST
