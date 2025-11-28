#include "log.h"
#include <iostream>
#include <sstream>

namespace {
    Log::Level g_level = Log::Level::Debug;
    std::mutex g_mtx;
}

namespace Log {

void init() {
    // default debug level
    g_level = Level::Debug;
}

void setLevel(Level l) { std::lock_guard<std::mutex> lk(g_mtx); g_level = l; }
Level getLevel() { std::lock_guard<std::mutex> lk(g_mtx); return g_level; }

void log(Level lvl, const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_mtx);
    if((int)lvl < (int)g_level) return;
    switch(lvl) {
        case Level::Debug: std::cout << "[DEBUG] "; break;
        case Level::Info:  std::cout << "[INFO]  "; break;
        case Level::Warn:  std::cout << "[WARN]  "; break;
        case Level::Error: std::cerr << "[ERROR] "; break;
    }
    if(lvl == Level::Error) std::cerr << msg << std::endl; else std::cout << msg << std::endl;
}

void logWithLoc(Level lvl, const char* file, int line, const std::string& msg) {
    std::ostringstream ss;
    ss << "(" << file << ":" << line << ") " << msg;
    log(lvl, ss.str());
}

} // namespace Log
