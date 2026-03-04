#pragma once

// #define ECHELON_DEBUG

#ifdef ECHELON_DEBUG
    #include "Echelon/Logger/Logger.hpp"
    #include "Echelon/Core/Base.hpp"

    namespace Echelon {
        // Core Logger — static per TU; only the EntryPoint TU initialises it.
        static Ref<Logger> s_CoreLogger;
    }

    #define INIT_ECHELON_LOGGER() ::Echelon::s_CoreLogger = ::Echelon::CreateRef<::Echelon::Logger>("ECHELON");\
                                  ::Echelon::s_CoreLogger->AddSink(::Echelon::ConsoleSink);\
                                  ::Echelon::s_CoreLogger->AddSink(::Echelon::FileSink("ECHELON.log"));

    #define ECHELON_LOG_TRACE(...) do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Trace(__VA_ARGS__); } while(0)
    #define ECHELON_LOG_INFO(...)  do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Info(__VA_ARGS__); } while(0)
    #define ECHELON_LOG_DEBUG(...) do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Debug(__VA_ARGS__); } while(0)
    #define ECHELON_LOG_WARN(...)  do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Warn(__VA_ARGS__); } while(0)
    #define ECHELON_LOG_ERROR(...) do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Error(__VA_ARGS__); } while(0)
    #define ECHELON_LOG_FATAL(...) do { if (::Echelon::s_CoreLogger) ::Echelon::s_CoreLogger->Fatal(__VA_ARGS__); } while(0)

#else
    #define INIT_ECHELON_LOGGER()
    #define ECHELON_LOG_TRACE(...)
    #define ECHELON_LOG_INFO(...)
    #define ECHELON_LOG_DEBUG(...)
    #define ECHELON_LOG_WARN(...)
    #define ECHELON_LOG_ERROR(...)
    #define ECHELON_LOG_FATAL(...)
    
#endif
