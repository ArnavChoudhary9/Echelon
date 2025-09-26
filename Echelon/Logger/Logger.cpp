#include "Logger.h"

using namespace Echelon;

#define SPDLOG_PATTERN_DEFAULT "%^[%H:%M:%S] [%n] %v [%l]%$"

Logger::Logger(const std::string& name) : m_Name(name) {
    m_Logger = spdlog::get(name);
    if (!m_Logger) {
        m_Logger = std::make_shared<spdlog::logger>(name, spdlog::sinks_init_list{});
        m_Logger->set_level(spdlog::level::trace);
        m_Logger->flush_on(spdlog::level::trace);
        m_Logger->set_pattern(SPDLOG_PATTERN_DEFAULT);

        // Register the logger with spdlog
        spdlog::register_logger(m_Logger);
    }
}

Logger::~Logger() {
    spdlog::drop(m_Name);
}

void Logger::Trace (const std::string& message) const { m_Logger->trace (message); }
void Logger::Info  (const std::string& message) const { m_Logger->info  (message); }
void Logger::Debug (const std::string& message) const { m_Logger->debug (message); }
void Logger::Warn  (const std::string& message) const { m_Logger->warn  (message); }
void Logger::Error (const std::string& message) const { m_Logger->error (message); }
void Logger::Fatal (const std::string& message) const { m_Logger->critical(message); }

void Logger::AddSink(const std::shared_ptr<Sink>& sink) {
    sink->set_pattern(SPDLOG_PATTERN_DEFAULT);
    m_Logger->sinks().push_back(sink->GetSink());
}
