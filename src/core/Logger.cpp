// ============================================================================
// src/core/Logger.cpp
// ============================================================================
#include "metagfx/core/Logger.h"

namespace metagfx {

void Logger::Init() {
    Log(LogLevel::Info, "Logger initialized");
}

void Logger::Log(LogLevel level, const std::string& message) {
    const char* color = GetLevelColor(level);
    const char* levelStr = GetLevelString(level);
    const char* reset = "\033[0m";
    
    // Print: [timestamp] [LEVEL]: message
    std::cout << color << "[" << GetTimestamp() << "] "
              << "[" << levelStr << "]: " << reset
              << message << std::endl;
}

const char* Logger::GetLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
        default:                return "UNKNOWN";
    }
}

const char* Logger::GetLevelColor(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:   return "\033[37m";  // White
        case LogLevel::Debug:   return "\033[36m";  // Cyan
        case LogLevel::Info:    return "\033[32m";  // Green
        case LogLevel::Warning: return "\033[33m";  // Yellow
        case LogLevel::Error:   return "\033[31m";  // Red
        case LogLevel::Fatal:   return "\033[35m";  // Magenta
        default:                return "\033[0m";   // Reset
    }
}

std::string Logger::GetTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

} // namespace metagfx
