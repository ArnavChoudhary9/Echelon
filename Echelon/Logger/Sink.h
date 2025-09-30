#pragma once

#include "spdlog/sinks/sink.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace Echelon {
    /**
     * @brief Wrapper class for spdlog sinks to manage logging outputs.
     * 
     */
    class Sink {
    private:
        std::shared_ptr<spdlog::sinks::sink> m_sink;
        
    public:
        /**
         * @brief Construct a new Sink object
         * 
         * @param sink 
         */
        Sink(std::shared_ptr<spdlog::sinks::sink> sink) : m_sink(sink) {}

        // Move constructor
        Sink(Sink&& other) noexcept : m_sink(std::move(other.m_sink)) {}
        
        ~Sink() = default;

        /**
         * @brief Get the Sink object
         * 
         * @return std::shared_ptr<spdlog::sinks::sink> 
         */
        std::shared_ptr<spdlog::sinks::sink> GetSink() const { return m_sink; }
        
        // These methods forward to the underlying spdlog sink
        void log(const spdlog::details::log_msg& msg) { m_sink->log(msg); }
        void flush() { m_sink->flush(); }
        void set_level(spdlog::level::level_enum log_level) { m_sink->set_level(log_level); }
        spdlog::level::level_enum level() const { return m_sink->level(); }
        bool should_log(spdlog::level::level_enum msg_level) const { return m_sink->should_log(msg_level); }

        void set_pattern(const std::string& pattern) { m_sink->set_pattern(pattern); }
        void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) { m_sink->set_formatter(std::move(sink_formatter)); }
    };

    // Predefined sinks

    // Default Console sink with color support
    static const std::shared_ptr<Sink> ConsoleSink = std::make_shared<Sink>(
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>()
    );

    /**
     * @brief Creates a file sink for logging to a file.
     * 
     * @param filename 
     * @return const std::shared_ptr<Sink> 
     */
    const std::shared_ptr<Sink> FileSink(const std::string& filename) {
        return std::make_shared<Sink>(
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true)
        );
    }
}
