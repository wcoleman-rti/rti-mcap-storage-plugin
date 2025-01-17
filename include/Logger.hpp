#ifndef RTI_STORAGEPLUGIN_MCAPLOGGER_HPP_
#define RTI_STORAGEPLUGIN_MCAPLOGGER_HPP_

#include <iostream>
#include <sstream>
#include <iomanip>
#include <mutex>
#include "LoggerHelper.hpp"

namespace rti {


class LoggerInterface {
public:
    enum LogLevel {
        SILENT,
        FATAL,
        ERROR,
        WARN,
        INFO,
        DEBUG
    };

    virtual void fatal(std::string_view message) = 0;
    virtual void error(std::string_view message) = 0;
    virtual void warn(std::string_view message) = 0;
    virtual void info(std::string_view message) = 0;
    virtual void debug(std::string_view message) = 0;
    virtual void log(LogLevel level, std::string_view message) = 0;
};

class StreamLoggerInterface : public LoggerInterface {
public:
    class LogStream {
    public:
        LogStream(LoggerInterface& logger, LogLevel level)
            : logger_(logger), level_(level), stream_() {}

        ~LogStream() {
            if (!stream_.str().empty()) {
                logger_.log(level_, stream_.str());
            }
        }

        LogStream& operator<<(LogLevel level) {
            flush();  // Flush any previous message
            level_ = level;
            return *this;
        }

        template <typename T>
        LogStream& operator<<(const T& value) {
            stream_ << value;
            return *this;
        }

    private:
        LoggerInterface& logger_;
        LogLevel level_;
        std::ostringstream stream_;

        void flush() {
            if (!stream_.str().empty()) {
                logger_.log(level_, stream_.str());
                stream_.str("");  // Clear the stream
                stream_.clear();
            }
        }
    };
};

}  // rti

namespace rti::recording::storage::mcap {


class NoOpLogger : public ::rti::StreamLoggerInterface {
public:
    NoOpLogger() = default;
    ~NoOpLogger() = default;
    
    inline void fatal(std::string_view ) override {}

    inline void error(std::string_view ) override {}

    inline void warn(std::string_view ) override {}

    inline void info(std::string_view ) override {}

    inline void debug(std::string_view ) override {}

    inline void log(LogLevel , std::string_view ) override {}
};

class Logger : public ::rti::LoggerInterface {
public:

    class LogStream {
    public:
        LogStream(Logger& logger, LogLevel level)
            : logger_(logger), level_(level), stream_() 
        {

        }

        ~LogStream() {
            if (!stream_.str().empty()) {
                logger_.log(level_, stream_.str());
            }
        }

        LogStream& operator<<(LogLevel level) 
        {
            flush();  // Flush any previous message
            level_ = level;
            return *this;
        }

        template <typename T>
        LogStream& operator<<(const T& value) 
        {
            stream_ << value;
            return *this;
        }

    private:
        Logger& logger_;
        LogLevel level_;
        std::ostringstream stream_;

        void flush() {
            if (!stream_.str().empty()) {
                logger_.log(level_, stream_.str());
                stream_.str("");  // Clear the stream
                stream_.clear();
            }
        }
    };

    ~Logger() 
    {

    };

    Logger(const LogLevel& level = LogLevel::WARN, std::ostream & os = std::cout) 
        : ostream_(&os), 
          mutex_(std::make_shared<std::mutex>()),
          current_level_(level) 
    {

    }    

    Logger(const Logger& other)
        : ostream_(nullptr), mutex_(nullptr) 
    {
        std::lock_guard<std::mutex> lock(*other.mutex_);
        mutex_ = other.mutex_;
        ostream_ = other.ostream_;
        current_level_ = other.current_level_;
    }

    Logger& operator=(const Logger& other) 
    { 
        if (this != &other) {
            std::lock_guard<std::mutex> lock(*other.mutex_);
            mutex_ = other.mutex_;
            ostream_ = other.ostream_;
            current_level_ = other.current_level_;
        }
        return *this;
    }

    inline bool operator==(const Logger& other) 
    {
        return (mutex_ == other.mutex_);
    }

    inline LogLevel level() const 
    {
        return current_level_;
    }

    inline void level(const LogLevel& level) 
    {
        std::lock_guard<std::mutex> lock(*mutex_);
        current_level_ = level;
    }

    inline std::ostream& os() const 
    {
        return *ostream_;
    }

    
    inline void fatal(std::string_view message) override 
    {
        log(FATAL, message);
    }
    
    inline void error(std::string_view message) override 
    {
        log(ERROR, message);
    }

    inline void warn(std::string_view message) override 
    {
        log(WARN, message);
    }

    inline void info(std::string_view message) override 
    {
        log(INFO, message);
    }

    inline void debug(std::string_view message) override 
    {
        log(DEBUG, message);
    }

    inline LogStream operator<<(LogLevel level) 
    {
        std::lock_guard<std::mutex> lock(*mutex_);
        return LogStream(*this, level);
    }

private:
    std::ostream* ostream_;
    std::shared_ptr<std::mutex> mutex_ = std::make_shared<std::mutex>();
    LogLevel current_level_;

    void log(LogLevel level, std::string_view message) 
    {
        std::lock_guard<std::mutex> lock(*mutex_);
        if (level <= current_level_) {
            *ostream_ << std::left << std::setw(9) << "[" + logLevelToString(level) + "] " << message << std::endl;
        }
        if (level <= FATAL) {
            throw std::runtime_error(message.data());
        }
    }

    std::string logLevelToString(LogLevel level) const 
    {
        switch (level) {
            case FATAL: return "FATAL";
            case ERROR: return "ERROR";
            case WARN:  return "WARN";
            case INFO:  return "INFO";
            case DEBUG: return "DEBUG";
            default: return "UNKNOWN";
        }
    }

};

}  // rti::recording::storage::mcap


#endif // RTI_STORAGEPLUGIN_MCAPLOGGER_HPP_