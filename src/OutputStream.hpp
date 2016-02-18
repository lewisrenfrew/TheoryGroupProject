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

/// Converts the logging output stream to output json messages, these
/// are use in conjunction with the GUI as newline delimited JSON.
/// This method creates an additional thread to prepare and emit the
/// messages
class JsonOutputStream
{
public:
    typedef std::lock_guard<std::mutex> ScopeLock;

    /// Constructor taking a pointer to the log object (there should only be one of these)
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

    /// Destructor, resets the output stream to its normal settings in case this happens to be removed before the program terminates
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

    /// Appends a message to the queue with the name of the function
    /// timed and how long it took
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

    /// Appends a message to the queue stating that a graph has been
    /// output to a certain file and a message with information about
    /// what the graph contains
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
    /// Split a string by delimiter, returning a vector split strings
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

    /// Split a string and by newline and return a vector of the
    /// produced messages
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

    /// Mutex for the message queue
    std::mutex queueLock_;
    /// Thread for logging
    std::thread logThread_;
    std::vector<std::string> messageQueue_;
    /// Stringstream onto which the Logfile class logs
    std::stringstream outStream_;
    /// Lock shared with the logfile class for not clobbering the stringstream
    std::mutex streamLock_;
    /// Pointer to the logfile instance
    Lethani::Logfile* dbg_;
    /// Flag to tell the message thread that we want to exit
    std::atomic<bool> exit_;
};

/// Handles reporting function analytics
class AnalyticsDaemon
{
public:
    /// Operation mode determines the output of the analytics, None,
    /// Normally to Stdout or a special JSON messages
    enum class Mode
    {
        None,
        StdOut,
        JSONStdOut
    };

    AnalyticsDaemon() : stream_(nullptr), mode_(Mode::None)
    {
    }

    /// Used to report the time taken by a timed function, called by
    /// the TIME_FUNCTION macro
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

    /// Used to send a message that a graph has been written to a
    /// certain filename along with additional information about the
    /// graph
    void
    ReportGraphOutput(const char* fileName, const char* message)
    {
        switch (mode_)
        {
        case Mode::None:
            break;

        case Mode::StdOut:
        {
            LOG("Plotted %s, %s", fileName, message);

        } break;

        case Mode::JSONStdOut:
        {
            stream_->EnqueueGraphOutput(fileName, message);
        }
        }
    }

    /// Changes the operation mode of the AnalyticsDaemon, a
    /// JsonOutputStream instance must be provided to log to JSON
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
