#ifndef LOGGER_H
#define LOGGER_H

#include "base.h"
#include "exceptions.h"
#include <string>
#include <mutex>

#ifdef _3FD_POCO_SUPPORT
#	include <Poco/Logger.h>
#	include <Poco/Message.h>
#elif defined _3FD_PLATFORM_WINRT
#	include "utils.h"
#	include <boost/lockfree/queue.hpp>
#	include <thread>
#	include <memory>
#endif

namespace _3fd
{
	using std::string;

	namespace core
	{
		/// <summary>
		/// Implements a logging facility.
		/// </summary>
		class Logger : notcopiable
		{
		public:

			/// <summary>
			/// Log priority enumeration (extracted from POCO C++ Foundation)
			/// </summary>
			enum Priority
			{
				PRIO_FATAL = 1,   /// A fatal error. The application will most likely terminate. This is the highest priority.
				PRIO_CRITICAL,    /// A critical error. The application might not be able to continue running successfully.
				PRIO_ERROR,       /// An error. An operation did not complete successfully, but the application as a whole is not affected.
				PRIO_WARNING,     /// A warning. An operation completed with an unexpected result.
				PRIO_NOTICE,      /// A notice, which is an information with just a higher priority.
				PRIO_INFORMATION, /// An informational message, usually denoting the successful completion of an operation.
				PRIO_DEBUG,       /// A debugging message.
				PRIO_TRACE        /// A tracing message. This is the lowest priority.
			};

		private:

#ifdef _3FD_POCO_SUPPORT
			Poco::Logger *m_logger;

#elif defined _3FD_PLATFORM_WINRT

			/// <summary>
			/// Represents a queued log event.
			/// </summary>
			struct Event
			{
				time_t time;
				Priority prio;
				string what;

#	ifdef ENABLE_3FD_ERR_IMPL_DETAILS
				string details;
#	endif
#	ifdef ENABLE_3FD_CST
				string cst;
#	endif
				Event(time_t p_time, Priority p_prio, string &&p_what)
					: time(p_time), prio(p_prio), what(std::move(p_what)) {}
			};

			std::thread m_logWriterThread;

			utils::Event m_terminationEvent;

			boost::lockfree::queue<Event *> m_eventsQueue;

			Windows::Storage::StorageFile ^m_txtLogFile;

			void LogWriterThreadProc();
#endif

			Logger(const string &id, bool logToConsole);

			// Singleton needs:

			static Logger *uniqueObjectPtr;

			static std::mutex singleInstanceCreationMutex;

			static void CreateInstance(const string &id, bool logToConsole);

			static Logger *GetInstance();

			// Private implementations:

			void WriteImpl(IAppException &ex, Priority prio);

			void WriteImpl(string &&message, Priority prio, bool cst);

			void WriteImpl(string &&what, string &&details, Priority prio, bool cst);

		public:

			static void Shutdown();

			~Logger();

			/// <summary>
			/// Writes an exception to the log output.
			/// </summary>
			/// <param name="message">The exception to log.</param>
			/// <param name="prio">The priority of the error.</param>
			static void Write(IAppException &ex, Priority prio)
			{
				Logger * const singleton = GetInstance();
				if (singleton != nullptr)
					singleton->WriteImpl(ex, prio);
			}

			/// <summary>
			/// Writes a message to the log output.
			/// </summary>
			/// <param name="message">The message to log.</param>
			/// <param name="prio">The priority of the message.</param>
			/// <param name="cst">When set to <c>true</c>, append the call stack trace.</param>
			static void Write(const string &message, Priority prio, bool cst = false)
			{
				Write(string(message), prio, cst);
			}

			/// <summary>
			/// Writes a message to the log output.
			/// </summary>
			/// <param name="message">The message to log.</param>
			/// <param name="prio">The priority of the message.</param>
			/// <param name="cst">When set to <c>true</c>, append the call stack trace.</param>
			static void Write(string &&message, Priority prio, bool cst = false)
			{
				Logger * const singleton = GetInstance();
				if (singleton != nullptr)
					singleton->WriteImpl(std::move(message), prio, cst);
			}

			/// <summary>
			/// Writes a message and its details to the log output.
			/// </summary>
			/// <param name="what">The reason for the message.</param>
			/// <param name="details">The message details.</param>
			/// <param name="prio">The priority for the message.</param>
			/// <param name="cst">When set to <c>true</c>, append the call stack trace.</param>
			static void Write(const string &what, const string &details, Priority prio, bool cst = false)
			{
				Write(string(what), string(details), prio, cst);
			}

			/// <summary>
			/// Writes a message and its details to the log output.
			/// </summary>
			/// <param name="what">The reason for the message.</param>
			/// <param name="details">The message details.</param>
			/// <param name="prio">The priority for the message.</param>
			/// <param name="cst">When set to <c>true</c>, append the call stack trace.</param>
			static void Write(string &&what, string &&details, Priority prio, bool cst = false)
			{
				Logger * const singleton = GetInstance();
				if (singleton != nullptr)
					singleton->WriteImpl(std::move(what), std::move(details), prio, cst);
			}
		};

	}// end of namespace core
}// end of namespace _3fd

#endif // end of header guard
