#include "stdafx.h"
#include "callstacktracer.h"
#include "configuration.h"
#include "exceptions.h"
#include "logger.h"
#include <sstream>

namespace _3fd
{
	namespace core
	{
		///////////////////////////////////////
		//  CallStack Class
		///////////////////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="CallStack" /> class.
		/// </summary>
		/// <param name="logInitialCap">The log initial capacity.</param>
		CallStack::CallStack(size_t logInitialCap)
		{
			m_stackFrames.reserve(logInitialCap);
		}

		/// <summary>
		/// Registers the frame.
		/// </summary>
		/// <param name="file">The file name.</param>
		/// <param name="line">The line number.</param>
		/// <param name="function">The function of the frame.</param>
		void CallStack::RegisterFrame(const char *file, 
									  unsigned long line, 
									  const char *function) NOEXCEPT
		{
			m_stackFrames.push_back(
				Frame(file, function, line)
			);
		}

		/// <summary>
		/// Pops the last added stack frame.
		/// </summary>
		/// <returns>Whether the stack log is empty after popping an entry from it.</returns>
		bool CallStack::PopStackFrameEntry() NOEXCEPT
		{
			m_stackFrames.pop_back();
			return m_stackFrames.empty();
		}

        // From path+fileName, gets only the file name
        static const char *GetFileName(const char *fullFileName)
        {
#       ifdef _WIN32
            const char filePathSeparator('\\');
#       else
            const char filePathSeparator('/');
#       endif
            const char *fileNameSubstr = strrchr(fullFileName, filePathSeparator);

            if (fileNameSubstr != nullptr)
                ++fileNameSubstr;
            else
                fileNameSubstr = fullFileName;

            return fileNameSubstr;
        }

		/// <summary>
		/// Gets the call stack trace report.
		/// </summary>
		/// <returns>A string which shows the current stack trace.</returns>
		string CallStack::GetReport()
		{
#       ifdef _3FD_PLATFORM_WINRT
            const char *newLine = "\n";
#       else
            const char *newLine = "\r\n";
#       endif
			std::ostringstream oss;

			for(int index = 0 ; index < m_stackFrames.size() ; ++index)
			{
				auto &frame = m_stackFrames[index];

				oss << "$ " << GetFileName(frame.file)
                    << " (" << frame.line << ") @ " << frame.function
                    << newLine;
			}

			return oss.str();
		}


		//////////////////////////////////
		//  CallStackTracer Class
		//////////////////////////////////

        thread_local_def CallStack * CallStackTracer::callStack(nullptr);

		/// <summary>
		/// Registers the current thread to have its stack traced.
		/// </summary>
		/// <returns>
        /// <see cref="STATUS_OKAY"/> whenever successful, otherwise, <see cref="STATUS_FAIL"/>
        /// </returns>
		bool CallStackTracer::RegisterThread()
		{
			if(callStack == nullptr)
			{
				try
				{
					callStack = new CallStack (
						AppConfig::GetSettings().framework.stackTracing.stackLogInitialCap
					);

                    return STATUS_OKAY;
				}
				catch(IAppException &appEx)
				{
                    // If the framework settings could not be initialized, this is a fatal error:
                    AttemptConsoleOutput(appEx.ToString());
                    exit(EXIT_FAILURE);
				}
				catch(std::exception &ex)
				{
					std::ostringstream oss;
					oss << "Generic failure when attempting to register thread for call stack tracing: " << ex.what();
                    AttemptConsoleOutput(oss.str());
				}

                return STATUS_FAIL;
			}
		}

		/// <summary>
		/// Unregisters the current thread for stack tracing.
		/// </summary>
		void CallStackTracer::UnregisterThread()
		{
			if(callStack != nullptr)
			{
				delete callStack;
				callStack = nullptr;
			}
		}

		/// <summary>
		/// Tracks the call.
		/// </summary>
		/// <param name="file">The file name.</param>
		/// <param name="line">The line number.</param>
		/// <param name="function">The function name.</param>
		void CallStackTracer::TrackCall(const char *file, 
										unsigned long line, 
										const char *function)
		{
			if(callStack != nullptr)
				callStack->RegisterFrame(file, line, function);
			else if (RegisterThread() == STATUS_OKAY)
                callStack->RegisterFrame(file, line, function);
		}

		/// <summary>
		/// Pops the last added stack frame.
		/// If the stack become empty, this object will be destroyed.
		/// </summary>
		void CallStackTracer::PopStackFrameEntry() NOEXCEPT
		{
			if(callStack->PopStackFrameEntry() == false)
				return;
			else
				UnregisterThread();
		}

		/// <summary>
		/// Gets the stack frame report.
		/// </summary>
		/// <returns>A text encoded report of the stack frame.</returns>
		string CallStackTracer::GetStackReport()
		{
			return callStack->GetReport();
		}

	}// end of namespace core
}// end of namespace _3fd
