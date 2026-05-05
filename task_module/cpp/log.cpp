#include "log.h"
#include <iostream>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

std::unique_ptr<Logger> Logger::s_instance_ = nullptr;

Logger::Logger() {}

Logger::Logger(const std::string& filename) {
    log_file_.open(filename, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Logger: Failed to open log file: " << filename << std::endl;
    }
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::init(const std::string& filename) {
    if (s_instance_) {
        // 已经初始化过，忽略
        return;
    }
    
    std::string outputDir = filename;
    if (!outputDir.empty() && outputDir.back() != '/') {
        outputDir += '/';
    }

    // 先尝试创建目录（如果不存在）
    if (mkdir(outputDir.c_str(), 0777) == -1 && errno != EEXIST) {
        std::cerr << "Logger: Failed to create output directory: " << outputDir << std::endl;
    }

    // 清空目录下的其他常规文件
    DIR* dir = opendir(outputDir.c_str());
    if (dir != nullptr) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            std::string fileName = ent->d_name;
            if (fileName != "." && fileName != ".." && ent->d_type == DT_REG) {
                std::string filePath = outputDir + fileName;
                remove(filePath.c_str());
            }
        }
        closedir(dir);
    }

    // 获取当前时间，生成唯一日志文件名
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y%m%d_%H%M%S", t);
    std::string logFilePath = outputDir + "log_" + timebuf + ".txt";

    s_instance_ = std::unique_ptr<Logger>(new Logger(logFilePath));
}

Logger& Logger::instance() {
    if (!s_instance_) {
        // 如果还没初始化，给出警告并用默认文件名
        std::cerr << "Logger::instance() called before Logger::init(), using default log.txt" << std::endl;
        s_instance_ = std::unique_ptr<Logger>(new Logger("log.txt"));
    }
    return *s_instance_;
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel = level;
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case DEBUG: return "DEBUG";
        case INFO: return "INFO";
        case WARNING: return "WARNING";
        case ERROR: return "ERROR";
        case CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const char* message) {
    if (level < currentLevel) return;

    std::lock_guard<std::mutex> lock(mtx_);
    std::time_t t = std::time(nullptr);
    char timebuf[32];
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    std::string log_line = std::string("[") + timebuf + "][" + levelToString(level) + "] " + message;

    std::cout << log_line << std::endl;
    if (log_file_.is_open()) {
        log_file_ << log_line << std::endl;
    }
}

void Logger::debug(const char* message)    { log(DEBUG, message); }
void Logger::info(const char* message)     { log(INFO, message); }
void Logger::warning(const char* message)  { log(WARNING, message); }
void Logger::error(const char* message)    { log(ERROR, message); }
void Logger::critical(const char* message) { log(CRITICAL, message); }
