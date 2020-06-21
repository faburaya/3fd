#ifndef _3FD_LOGGER_H
#define _3FD_LOGGER_H

#include <3fd/core/exceptions.h>
#include <3fd/utils/concurrency.h>
#include <3fd/utils/lockfreequeue.h>

#include <cinttypes>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace _3fd
{
namespace core
{
    using std::string;

    void AttemptConsoleOutput(const string &message);

    /// <summary>
    /// Common interface (POSIX/Win32/WinRT) for file access.
    /// </summary>
    class INTFOPT ILogFileAccess
    {
    public:

        virtual ~ILogFileAccess() = default;

        /// <summary>
        /// Gets the output stream.
        /// </summary>
        virtual std::ostream &GetStream() = 0;

        /// <summary>
        /// Tells whether the stream is in a bad state.
        /// </summary>
        virtual bool HasError() const = 0;

        /// <summary>
        /// Switches log stream to a new file.
        /// </summary>
        /// <param name="ofs">Will receive an output stream to the new file.</param>
        virtual void ShiftToNewLogFile() = 0;

        virtual uint64_t GetFileSize() const = 0;
    };

    std::unique_ptr<ILogFileAccess> GetFileAccess(const string &loggerId);

#ifdef _3FD_CONSOLE_AVAILABLE
    std::unique_ptr<ILogFileAccess> GetConsoleAccess();
#endif

    /// <summary>
    /// Implements a logging facility.
    /// </summary>
    class Logger
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

        /// <summary>
        /// Represents a queued log event.
        /// </summary>
        struct LogEvent
        {
            time_t time;
            Priority prio;
            string what;

#    ifdef ENABLE_3FD_ERR_IMPL_DETAILS
            string details;
#    endif
#    ifdef ENABLE_3FD_CST
            string trace;
#    endif
            LogEvent(time_t p_time, Priority p_prio, string &&p_what)
                : time(p_time), prio(p_prio), what(std::move(p_what)) {}

            LogEvent(LogEvent &&ob) :
                time(ob.time),
                prio(ob.prio),
                what(std::move(ob.what))
#    ifdef ENABLE_3FD_ERR_IMPL_DETAILS
                , details(std::move(ob.details))
#    endif
#    ifdef ENABLE_3FD_CST
                , trace(std::move(ob.trace))
#    endif
            {}
        };

        std::thread m_logWriterThread;

        utils::Event m_terminationEvent;

        utils::LockFreeQueue<LogEvent> m_eventsQueue;

        std::unique_ptr<ILogFileAccess> m_fileAccess;

        Priority m_prioThreshold;

        void LogWriterThreadProc();

        Logger(const string &id, bool logToConsole);

        // Singleton needs:

        static Logger *uniqueObjectPtr;

        static std::mutex singleInstanceCreationMutex;

        static void CreateInstance(const string &id, bool logToConsole);

        static Logger *GetInstance() noexcept;

        // Private implementations:

        void WriteImpl(const IAppException &ex, Priority prio);

#if defined _3FD_PLATFORM_WIN32API || defined _3FD_PLATFORM_WINRT_UWP
        void WriteImpl(HRESULT hr, const char *message, const char *function, Priority prio);
#endif
        void WriteImpl(string &&message, Priority prio, bool cst) noexcept;

        void WriteImpl(string &&what, string &&details, Priority prio, bool cst) noexcept;

    public:

        static void Shutdown();

        Logger(const Logger &) = delete;

        ~Logger();

        /// <summary>
        /// Writes an exception to the log output.
        /// </summary>
        /// <param name="message">The exception to log.</param>
        /// <param name="prio">The priority of the error.</param>
        static void Write(const IAppException &ex, Priority prio)
        {
            Logger * const singleton = GetInstance();
            if (singleton != nullptr)
                singleton->WriteImpl(ex, prio);
        }

#if defined _3FD_PLATFORM_WIN32API || defined _3FD_PLATFORM_WINRT_UWP
        /// <summary>
        /// Writes an HRESULT error to the log output.
        /// </summary>
        /// <param name="hr">The HRESULT code.</param>
        /// <param name="message">The main error message.</param>
        /// <param name="function">The name function of the function that returned the error code.</param>
        /// <param name="prio">The priority of the message.</param>
        static void Write(HRESULT hr, const char *message, const char *function, Priority prio)
        {
            Logger * const singleton = GetInstance();
            if (singleton != nullptr)
                singleton->WriteImpl(hr, message, function, prio);
        }
#endif
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
        static void Write(string &&message, Priority prio, bool cst = false) noexcept
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

    std::ostream &PrepareEventString(std::ostream &ofs, time_t timestamp, Logger::Priority prio);

    /// <summary>
    /// Writes a message to the log upon end of scope, appending a
    /// given suffix for success or failure depending on the situation.
    /// </summary>
    class ScopedLogWrite
    {
    private:

        string m_message;

        Logger::Priority m_prioWhenSuccess;
        Logger::Priority m_prioWhenFailure;

        const char *m_suffixWhenSuccess;
        const char *m_suffixWhenFailure;

        bool m_wasFailure;

    public:

        ScopedLogWrite(const ScopedLogWrite &) = delete;

        /// <summary>
        /// Initializes a new instance of the <see cref="ScopedLogWrite"/> class.
        /// </summary>
        /// <param name="oss">The string buffer containing the message (prefix).</param>
        /// <param name="prioWhenSuccess">The log priority when success.</param>
        /// <param name="suffixWhenSuccess">The suffix to append when success.</param>
        /// <param name="prioWhenFailure">The log priority when failure.</param>
        /// <param name="suffixWhenFailure">The suffix to append when failure.</param>
        ScopedLogWrite(const string &message,
                       Logger::Priority prioWhenSuccess,
                       const char *suffixWhenSuccess,
                       Logger::Priority prioWhenFailure,
                       const char *suffixWhenFailure) :
            m_message(message),
            m_prioWhenSuccess(prioWhenSuccess),
            m_suffixWhenSuccess(suffixWhenSuccess),
            m_prioWhenFailure(prioWhenFailure),
            m_suffixWhenFailure(suffixWhenFailure),
            m_wasFailure(true)
        {};

        /// <summary>
        /// Finalizes an instance of the <see cref="ScopedLogWrite"/> class.
        /// </summary>
        ~ScopedLogWrite()
        {
            if (m_wasFailure)
                Logger::Write(m_message.append(m_suffixWhenFailure), m_prioWhenFailure);
        }

        /// <summary>
        /// Writes the message to the log with the suffix for success.
        /// </summary>
        void LogSuccess()
        {
            Logger::Write(m_message.append(m_suffixWhenSuccess), m_prioWhenSuccess);
            m_wasFailure = false;
        }
    };

}// end of namespace core
}// end of namespace _3fd

#endif // end of header guard
