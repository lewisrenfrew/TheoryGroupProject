// -*- c++ -*-
#if !defined(OUTPUTSTREAM_H)
/* ==========================================================================
   $File: OutputStream.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define OUTPUTSTREAM_H
#include "GlobalDefines.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <mutex>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

class JsonOutputStream
{
public:
    typedef std::lock_guard<std::mutex> ScopeLock;

    JsonOutputStream(Lethani::Logfile* dbg)
        : queueLock_(),
          logThread_(),
          messageQueue_(),
          outStream_(),
          streamLock_(),
          dbg_(dbg),
          exit_(false)
    {
        dbg_->LogConsoleToProtectedStream(&outStream_, &streamLock_);
        logThread_ = std::thread(&JsonOutputStream::LogLoop, this);
    }

    ~JsonOutputStream()
    {
        exit_ = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        logThread_.join();
        dbg_->ResetToStdOut();
    }

    /// Function running in the output logging loop
    void
    LogLoop()
    {
        while (true)
        {
            // Check outStream
            // Convert and append to messages
            // empty stream
            // Check message queue
            // output and empty queue
            {
                ScopeLock lock(streamLock_);
                if (outStream_.str().length() > 0)
                {
                    EnqueueMessage(outStream_.str());
                }
                outStream_.str("");
            }

            {
                ScopeLock lock(queueLock_);
                for (const auto& msg : messageQueue_)
                {
                    // We want to flush between each message
                    std::cout << msg << std::endl;
                }
                messageQueue_.resize(0);
            }

            // Check this here to get a final print after exit is
            // called if we're in the process of sleeping
            if (exit_)
                break;

            // NOTE(Chris): A condition variable might be better, but I'm lazy
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    /// Appends a string message to the queue, processing as appropriate
    void
    EnqueueMessage(const std::string& msg)
    {
        ScopeLock lock(queueLock_);
        for (const auto& s : StringToMessage(msg))
        {
            messageQueue_.push_back(s);
        }
    }

    void
    EnqueueFunctionTimeData(const char* fnName,
                            const std::chrono::milliseconds duration)
    {
        std::string msg("{ \"type\" : \"timing\", \"function\" : \"");
        msg += fnName;
        msg += "\", \"duration\" : ";
        msg += std::to_string(duration.count()).c_str();
        msg += "}";

        ScopeLock lock(queueLock_);
        messageQueue_.push_back(msg);
    }

    void
    EnqueueGraphOutput(const char* filename, const char* message)
    {
        std::string msg("{ \"type\" : \"graph\", \"file\" : \"");
        msg += filename;
        msg += "\", \"message\" : \"";
        msg += message;
        msg += "\" }";

        ScopeLock lock(queueLock_);
        messageQueue_.push_back(msg);
    }

private:
    static std::vector<std::string>
    SplitString(const std::string& str, const char delim)
    {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string val;
        while (std::getline(ss, val, delim))
        {
            if (!val.empty())
                result.push_back(val);
        }
        return result;
    }

    static std::vector<std::string>
    StringToMessage(const std::string& str)
    {
        // Do some json conversion
        const auto strs = SplitString(str, '\n');
        std::vector<std::string> result;
        for (const auto& s : strs)
        {
            std::string msg("{ \"type\" : \"message\", \"message\" : \"");
            msg += s;
            msg += "\" }";
            result.push_back(msg);
        }
        return result;
    }

    std::mutex queueLock_;
    std::thread logThread_;
    std::vector<std::string> messageQueue_;
    std::stringstream outStream_;
    std::mutex streamLock_;
    Lethani::Logfile* dbg_;
    std::atomic<bool> exit_;
};

class AnalyticsDaemon
{
public:
    enum class Mode
    {
        None,
        StdOut,
        JSONStdOut
    };

    AnalyticsDaemon() : stream_(nullptr), mode_(Mode::None)
    {
    }

    void
    ReportTimedFunction(const char* fnName,
                        const std::chrono::milliseconds duration)
    {
        switch (mode_)
        {
        case Mode::None:
            break;

        case Mode::StdOut:
        {
            LOG("Function %s, took %s ms", fnName,
                std::to_string(duration.count()).c_str());

        } break;

        case Mode::JSONStdOut:
        {
            stream_->EnqueueFunctionTimeData(fnName, duration);
        }
        }
    }

    void
    ReportGraphOutput(const char* fileName, const char* message)
    {
    }

    void SetMode(Mode mode, JsonOutputStream* str = nullptr)
    {
        if (mode == Mode::JSONStdOut && str == nullptr)
            return;

        stream_ = str;
        mode_ = mode;
    }

private:
    JsonOutputStream* stream_;
    Mode mode_;

};


#endif
