#pragma once

#include "../Core/Base.h"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/fmt.h"  // for fmt::runtime
#include "Sink.h"
#include <string>
#include <utility> // for std::forward

namespace Echelon {
    /**
     * @brief Logger class for logging messages with various severity levels.
     * 
     */
    class Logger {
    public:
        /**
         * @brief Construct a new Logger object
         * 
         * @param name The name of the logger.
         */
        Logger(const std::string&);
        ~Logger();

        void Trace (const std::string&) const;
        void Info  (const std::string&) const;
        void Debug (const std::string&) const;
        void Warn  (const std::string&) const;
        void Error (const std::string&) const;
        void Fatal (const std::string&) const;

        // Formatting overloads (runtime-checked) - use fmt::runtime to force runtime formatting
        template<typename... Args>
        void Trace(const std::string& fmt, Args&&... args) const { m_Logger->trace(fmt::runtime(fmt), std::forward<Args>(args)...); }

        template<typename... Args>
        void Info(const std::string& fmt, Args&&... args) const { m_Logger->info(fmt::runtime(fmt), std::forward<Args>(args)...); }

        template<typename... Args>
        void Debug(const std::string& fmt, Args&&... args) const { m_Logger->debug(fmt::runtime(fmt), std::forward<Args>(args)...); }

        template<typename... Args>
        void Warn(const std::string& fmt, Args&&... args) const { m_Logger->warn(fmt::runtime(fmt), std::forward<Args>(args)...); }

        template<typename... Args>
        void Error(const std::string& fmt, Args&&... args) const { m_Logger->error(fmt::runtime(fmt), std::forward<Args>(args)...); }

        template<typename... Args>
        void Fatal(const std::string& fmt, Args&&... args) const { m_Logger->critical(fmt::runtime(fmt), std::forward<Args>(args)...); }

        void AddSink(const Ref<Sink>&);

    private:
        std::string m_Name;
        Ref<spdlog::logger> m_Logger;
    };
}
