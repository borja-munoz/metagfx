// ============================================================================
// include/metagfx/core/Logger.h
// ============================================================================
#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <iomanip>

namespace metagfx {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

class Logger {
public:
    static void Init();
    
    static void Log(LogLevel level, const std::string& message);
    
    template<typename... Args>
    static void LogFormatted(LogLevel level, const char* format, Args... args);
    
private:
    static const char* GetLevelString(LogLevel level);
    static const char* GetLevelColor(LogLevel level);
    static std::string GetTimestamp();
};

// Stream-based logger helper
class LogStream {
public:
    LogStream(LogLevel level) : m_Level(level) {}
    
    ~LogStream() {
        Logger::Log(m_Level, m_Stream.str());
    }
    
    template<typename T>
    LogStream& operator<<(const T& value) {
        m_Stream << value;
        return *this;
    }
    
private:
    LogLevel m_Level;
    std::ostringstream m_Stream;
};

} // namespace metagfx

// Logging macros - create temporary LogStream objects
#define METAGFX_TRACE    ::metagfx::LogStream(::metagfx::LogLevel::Trace)
#define METAGFX_DEBUG    ::metagfx::LogStream(::metagfx::LogLevel::Debug)
#define METAGFX_INFO     ::metagfx::LogStream(::metagfx::LogLevel::Info)
#define METAGFX_WARN     ::metagfx::LogStream(::metagfx::LogLevel::Warning)
#define METAGFX_ERROR    ::metagfx::LogStream(::metagfx::LogLevel::Error)
#define METAGFX_CRITICAL ::metagfx::LogStream(::metagfx::LogLevel::Fatal)

