/* ==========================================================================
   $File: Debug.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctype.h>

#include "Debug.hpp"
#include "../GlobalDefines.hpp"

namespace Lethani {

	Logfile::Logfile(const char* filename) : consoleStream_(nullptr),
											 streamLock_(nullptr),
											 logfileGood_(true)
	{
		logfile_ = fopen(strlen(filename) != 0 ? filename : Version::DefaultLog, "w");
		if (logfile_ == nullptr)
		{
			printf("Logfile creation failed\n");
			logfileGood_ = false;
		}
	}

	Logfile::Logfile(const std::string& filename) : consoleStream_(nullptr),
                                                    streamLock_(nullptr),
													logfileGood_(true)
	{
		logfile_ = fopen(filename.length() != 0 ? filename.c_str() : Version::DefaultLog, "w");
		if (logfile_ == nullptr)
		{
			printf("Logfile creation failed\n");
			logfileGood_ = false;
		}
	}

	Logfile::Logfile() : consoleStream_(nullptr),
                         streamLock_(nullptr),
						 logfileGood_(true)
	{
		logfile_ = fopen(Version::DefaultLog, "w");
		if (logfile_ == nullptr)
		{
			printf("Logfile creation failed\n");
			logfileGood_ = false;
		}
	}

	Logfile::~Logfile()
	{
		logfileGood_ = false;
		fflush(stdout);
		fflush(logfile_);
		fclose(logfile_);
	}

	void Logfile::DebugPrintfVargs(const char* _format, va_list _argList)
	{
		// NOTE(Chris): 8k limitation has never been a problem so far
		char temp[8192];
		char* out = temp;
		i32 len = vsnprintf(out, sizeof(temp), _format, _argList);
		if ((i32)sizeof(temp) < len)
		{
			out = (char*)calloc(len+1, sizeof(char));
			len = vsnprintf(out, len, _format, _argList);
		}
		out[len] = '\0';
        if (streamLock_ != nullptr)
        {
            std::lock_guard<std::mutex> lockGuard(*streamLock_);
            *consoleStream_ << out;
        }
        else
        {
            fputs(out, stdout);
            fflush(stdout);
        }
		fputs(out, logfile_);
		fflush(logfile_);
	}

	void Logfile::DebugPrintfVargsStat(const char* _format, va_list _argList)
	{
		// NOTE(Chris): 8k limitation has never been a problem so far
		char temp[8192];
		char* out = temp;
		i32 len = vsnprintf(out, sizeof(temp), _format, _argList);
		if ((i32)sizeof(temp) < len)
		{
			out = (char*)calloc(len+1, sizeof(char));
			len = vsnprintf(out, len, _format, _argList);
		}
		out[len] = '\0';
		fputs(out, stdout);
		fflush(stdout);
	}

    void Logfile::LogConsoleToProtectedStream(std::stringstream *sstream, std::mutex *lock)
    {
            consoleStream_ = sstream;
            streamLock_ = lock;
    }

	void Logfile::ResetToStdOut()
	{
		{
			std::lock_guard<std::mutex> lock(*streamLock_);
			consoleStream_ = nullptr;
		}
		streamLock_ = nullptr;
	}

	void Logfile::DebugPrintf(const char* _format, ...)
	{
		va_list argList;
		va_start(argList, _format);
		DebugPrintfVargs(_format, argList);
		va_end(argList);
	}

	void Logfile::DebugPrintfStat(const char *_format, ...)
	{
		va_list argList;
		va_start(argList, _format);
		Logfile::DebugPrintfVargsStat(_format, argList);
		va_end(argList);
	}

}
