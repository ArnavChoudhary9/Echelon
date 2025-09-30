#pragma once

// #define ECHELON_DEBUG

#ifdef ECHELON_DEBUG
    #include "../Logger/Logger.h"
    #include "../Core/Base.h"

    namespace Echelon {
        // Core Logger
        static Ref<Logger> s_CoreLogger;
    }

    #define INIT_ECHELON_LOGGER() ::Echelon::s_CoreLogger = ::Echelon::CreateRef<::Echelon::Logger>("ECHELON");\
                                  ::Echelon::s_CoreLogger->AddSink(::Echelon::ConsoleSink);\
                                  ::Echelon::s_CoreLogger->AddSink(::Echelon::FileSink("ECHELON.log"));

    #define ECHELON_LOG_TRACE(...) ::Echelon::s_CoreLogger->Trace(__VA_ARGS__)
    #define ECHELON_LOG_INFO(...)  ::Echelon::s_CoreLogger->Info(__VA_ARGS__)
    #define ECHELON_LOG_DEBUG(...) ::Echelon::s_CoreLogger->Debug(__VA_ARGS__)
    #define ECHELON_LOG_WARN(...)  ::Echelon::s_CoreLogger->Warn(__VA_ARGS__)
    #define ECHELON_LOG_ERROR(...) ::Echelon::s_CoreLogger->Error(__VA_ARGS__)
    #define ECHELON_LOG_FATAL(...) ::Echelon::s_CoreLogger->Fatal(__VA_ARGS__)

#else
    #define INIT_ECHELON_LOGGER()
    #define ECHELON_LOG_TRACE(...)
    #define ECHELON_LOG_INFO(...)
    #define ECHELON_LOG_DEBUG(...)
    #define ECHELON_LOG_WARN(...)
    #define ECHELON_LOG_ERROR(...)
    #define ECHELON_LOG_FATAL(...)
    
#endif
