// -*- c++ -*-
#if !defined(DEBUG_H)
/* ==========================================================================
   $File: Debug.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   $Misc: Inspired by bgfx's dbg by Branimir Karadzic $
   ========================================================================== */
// NOTE(Chris): This is not good code really, but it functions for now
// NOTE(Chris): This may need refactoring to a C++ file access model
// if the current one does not prove thread safe/ordered - seems fine

#define DEBUG_H

#include <stdarg.h>
#include <stdint.h>
#include <string>
#include <sstream>
#include <mutex>

#define DEBUG_STRINGIFY(_x) DEBUG_STRINGIFY_(_x)
#define DEBUG_STRINGIFY_(_x) #_x

// NOTE(Chris): I don't believe MSVC provides __func__ (event though
// it's in C++11) and I've never touched ICC. If you're using a
// different compiler and know it has func for improved logging, add
// it to the list
#if defined(__GNUC__) || defined(__clang__)
#define DEBUG_FILE_LINE_LITERAL "" __FILE__ "(" DEBUG_STRINGIFY(__LINE__) ")<%s>: "
/// Logs without any file or function location data - through the default logging instance
#define SHORTLOG(_format, ...) Log::log.DebugPrintf("" _format "\n", ##__VA_ARGS__)
/// Logs with file and function location data - through the default logging instance
#define LOG(_format, ...) Log::log.DebugPrintf(DEBUG_FILE_LINE_LITERAL "" _format "\n", __func__,##__VA_ARGS__)
/// Logs with file and function data through the named logging instance
#define LOGNAMED(className, _format, ...) className.DebugPrintf(DEBUG_FILE_LINE_LITERAL "" _format "\n", __func__,##__VA_ARGS__)
// #define LOGSCREEN(_format, ...) Lethani::Logfile::DebugPrintfStat(DEBUG_FILE_LINE_LITERAL "" _format "\n", __func__,##__VA_ARGS__)
// #define SHORTLOGSCREEN(_format, ...) Lethani::Logfile::DebugPrintfStat(_format "\n", __func__,##__VA_ARGS__)
#else
#define DEBUG_FILE_LINE_LITERAL "" __FILE__ "(" DEBUG_STRINGIFY(__LINE__) "): "
#define LOG(_format, ...) Log::log.DebugPrintf(DEBUG_FILE_LINE_LITERAL "" _format "\n", ##__VA_ARGS__)
#define LOGNAMED(className, _format, ...) className.DebugPrintf(DEBUG_FILE_LINE_LITERAL "" _format "\n", ##__VA_ARGS__)
// #define LOGSCREEN(_format, ...) Lethani::Logfile::DebugPrintfStat(DEBUG_FILE_LINE_LITERAL "" _format "\n", ##__VA_ARGS__)
// #define SHORTLOGSCREEN(_format, ...) Lethani::Logfile::DebugPrintfStat(_format "\n", __func__,##__VA_ARGS__)
#endif


#ifndef NDEBUG
/// Logging function that is disabled when not in DEBUG mode (for performance critical zones)
#define DEBUGLOG(_format, ...) LOG(_format, __VA_ARGS__)
#else
#define DEBUGLOG(_format, ...)
#endif

namespace Lethani
{
    class Logfile
    {

    public:
        /// Arg-less constructor, logs to default file
        Logfile();
        /// Construct with specified filename
        Logfile(const char* filename);
        /// Construct with specified filename
        Logfile(const std::string& filename);
        ~Logfile();

        /// Change console output to a stringstream protected by a mutex
        void LogConsoleToProtectedStream(std::stringstream* sstream, std::mutex* lock);

        /// Resets the output from Protected stream to stdout
        void ResetToStdOut();

        /// Prints to stdout and filename
        void DebugPrintf(const char* _format, ...);
        /// Static method printing only to stdout, can be called
        /// anywhere the header is included
        static void DebugPrintfStat(const char* _format, ...);

        /// Returns whether the class instance has been initialised
        /// and is read to log to the file
        bool logfileReady() const { return logfileGood_; }

    private:
        /// Method used by DebugPrintf to to the actual logging
        void DebugPrintfVargs(const char* _format, va_list _argList);
        /// Method used by DebugPrintfStat to to the actual logging
        static void DebugPrintfVargsStat(const char* _format, va_list _argList);
        /// C-Style FILE pointer to logfile
        FILE* logfile_;
        /// Stringstreamfor console output
        std::stringstream* consoleStream_;
        /// Mutex to protecting stringstream (if used)
        std::mutex* streamLock_;
        /// Indicates whether instance is initialised with a valid file
        bool logfileGood_;
    };
}

#endif
