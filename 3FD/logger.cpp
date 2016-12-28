#include "stdafx.h"
#include "logger.h"
#include "configuration.h"

#include <iostream>
#include <stack>
#include <array>
#include <chrono>
#include <ctime>

namespace _3fd
{
namespace core
{
    /// <summary>
    /// Attempts to output a message to the console, if that exists and
    /// is available in the current platform. This is useful as a last
    /// resort when framework routines failed to initialize and something
    /// should be reported to the end user.
    /// </summary>
    /// <param name="message">The message.</param>
    void AttemptConsoleOutput(const string &message)
    {/* As standard procedure in this framework, exceptions thrown inside destructors are swallowed and logged.
        So, when this functions is invoked, exceptions cannot be forwarded up in the stack, because the original
        caller might be a destructor. The 'catch' clause below attempts to output the event in the console (if
        available). */
#ifdef _3FD_CONSOLE_AVAILABLE
        using namespace std::chrono;
        std::array<char, 21> buffer;
        auto now = system_clock::to_time_t(system_clock::now());
        strftime(buffer.data(), buffer.size(), "%Y-%b-%d %H:%M:%S", localtime(&now));
        std::clog << "@(" << buffer.data() << ")\t" << message << std::endl;
#endif
    }

    //////////////////////////////
    // Logger Class
    //////////////////////////////

	Logger * Logger::uniqueObjectPtr(nullptr);

	std::mutex Logger::singleInstanceCreationMutex;

	/// <summary>
	/// Creates the unique instance of the <see cref="Logger" /> class.
	/// </summary>
	/// <returns>A smart pointer to the singleton.</returns>
	void Logger::CreateInstance(const string &id, bool logToConsole)
	{
		using namespace std::chrono;

		try
		{
			std::lock_guard<std::mutex> lock(singleInstanceCreationMutex);

			if(uniqueObjectPtr == nullptr)
				uniqueObjectPtr = new Logger (id, logToConsole);
		}
		catch(std::system_error &ex)
		{
			throw core::AppException<std::runtime_error>(core::StdLibExt::GetDetailsFromSystemError(ex));
		}
		catch(std::bad_alloc &xa)
		{
			throw core::AppException<std::runtime_error>(xa.what());
		}
	}

	/// <summary>
	/// Shuts down the logger releasing all associated resources.
	/// </summary>
	void Logger::Shutdown()
	{
		if(uniqueObjectPtr != nullptr)
		{
			try
			{
				std::lock_guard<std::mutex> lock(singleInstanceCreationMutex);
				delete uniqueObjectPtr;
				uniqueObjectPtr = nullptr;
			}
			catch (std::system_error)
			{/* DO NOTHING: SWALLOW EXCEPTION
				This method cannot throw an exception because it could have been originally 
				invoked by a destructor. When that happens, memory leaks are expected. */
			}
		}
	}

	/// <summary>
	/// Writes an exception to the log output.
	/// </summary>
	/// <param name="message">The exception to log.</param>
	/// <param name="prio">The priority of the error.</param>
	void Logger::WriteImpl(IAppException &ex, Priority prio)
	{
		if(ex.GetInnerException() != nullptr)
		{
			std::stack<std::shared_ptr<IAppException>> lifo;
			auto item = ex.GetInnerException();

			do
			{
				lifo.push(item);
				item = item->GetInnerException();
			}
			while(item != nullptr);

			do
			{
				auto item = lifo.top();
				WriteImpl(item->ToString(), prio, false);
				lifo.pop();
			}
			while(lifo.empty() == false);
		}
			
		WriteImpl(ex.ToString(), prio, false);
	}

	/// <summary>
	/// Writes a message to the log output.
	/// </summary>
	/// <param name="message">The message to log.</param>
	/// <param name="prio">The priority of the message.</param>
	/// <param name="cst">When set to <c>true</c>, append the call stack trace.</param>
	void Logger::WriteImpl(string &&message, Priority prio, bool cst)
	{
		WriteImpl(std::move(message), string(""), prio, cst);
	}

}// end of namespace core
}// end of namespace _3fd
