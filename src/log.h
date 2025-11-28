#pragma once

#include <string>
#include <mutex>
#include <sstream>

namespace Log {
    enum class Level { Debug = 0, Info = 1, Warn = 2, Error = 3 };

    void init();
    void setLevel(Level l);
    Level getLevel();

    void log(Level lvl, const std::string& msg);
    void logWithLoc(Level lvl, const char* file, int line, const std::string& msg);
}

// Convenience macros
#define LOG_DEBUG(msg) do { std::ostringstream _log_ss; _log_ss << msg; Log::logWithLoc(Log::Level::Debug, __FILE__, __LINE__, _log_ss.str()); } while(0)
#define LOG_INFO(msg)  do { std::ostringstream _log_ss; _log_ss << msg; Log::logWithLoc(Log::Level::Info, __FILE__, __LINE__, _log_ss.str()); } while(0)
#define LOG_WARN(msg)  do { std::ostringstream _log_ss; _log_ss << msg; Log::logWithLoc(Log::Level::Warn, __FILE__, __LINE__, _log_ss.str()); } while(0)
#define LOG_ERROR(msg) do { std::ostringstream _log_ss; _log_ss << msg; Log::logWithLoc(Log::Level::Error, __FILE__, __LINE__, _log_ss.str()); } while(0)
