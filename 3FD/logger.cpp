#include "stdafx.h"
#include "logger.h"
#include "configuration.h"

#include <stack>

namespace _3fd
{
	namespace core
	{
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
