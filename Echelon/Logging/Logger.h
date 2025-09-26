#pragma once

#include "spdlog/spdlog.h"
#include "Sink.h"
#include <string>

namespace Echelon {
    class Logger {
    public:
        Logger(const std::string& name);
        ~Logger();

        void Info (const std::string& message) const;
        void Debug(const std::string& message) const;
        void Warn(const std::string& message) const;
        void Error(const std::string& message) const;
        void Fatal(const std::string& message) const;

        void AddSink(const std::shared_ptr<Sink>& sink);

    private:
        std::string m_Name;
        std::shared_ptr<spdlog::logger> m_Logger;
    };
}
