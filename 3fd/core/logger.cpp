#include "pch.h"
#include "preprocessing.h"

#if defined _3FD_PLATFORM_WIN32API || defined _3FD_PLATFORM_WINRT_UWP
/* Need to place COM headers here, because some lines
   below POCO interferes undefining Windows macros */
#   include <comdef.h>
#else
#   include <unistd.h>
#endif

#include "logger.h"
#include "configuration.h"
#include "callstacktracer.h"

#include <array>
#include <codecvt>
#include <chrono>
#include <ctime>
#include <future>
#include <stack>

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
    {
#ifdef _3FD_CONSOLE_AVAILABLE
        using namespace std::chrono;
        std::array<char, 21> buffer;
        auto now = system_clock::to_time_t(system_clock::now());
        strftime(buffer.data(), buffer.size(), "%Y-%b-%d %H:%M:%S", localtime(&now));
        std::clog << "@(" << buffer.data() << ")\t" << message << std::endl;
#endif
    }

    /// <summary>
    /// Prepares the log event string.
    /// </summary>
    /// <param name="oss">The file stream output.</param>
    /// <param name="timestamp">The event timestamp.</param>
    /// <param name="prio">The event priority.</param>
    /// <returns>A reference to the string stream output.</returns>
    std::ostream &PrepareEventString(std::ostream &ofs, time_t timestamp, Logger::Priority prio)
    {
#ifdef _WIN32
        static const auto pid = GetCurrentProcessId();
#else
        static const auto pid = getpid();
#endif
        std::array<char, 21> buffer;
        strftime(buffer.data(), buffer.size(), "%Y-%b-%d %H:%M:%S", localtime(&timestamp));
        ofs << buffer.data() << " [process " << pid;

        switch (prio)
        {
        case Logger::PRIO_FATAL:
            ofs << "] - FATAL - ";
            break;

        case Logger::PRIO_CRITICAL:
            ofs << "] - CRITICAL - ";
            break;

        case Logger::PRIO_ERROR:
            ofs << "] - ERROR - ";
            break;

        case Logger::PRIO_WARNING:
            ofs << "] - WARNING - ";
            break;

        case Logger::PRIO_NOTICE:
            ofs << "] - NOTICE - ";
            break;

        case Logger::PRIO_INFORMATION:
            ofs << "] - INFORMATION - ";
            break;

        case Logger::PRIO_DEBUG:
            ofs << "] - DEBUG - ";
            break;

        case Logger::PRIO_TRACE:
            ofs << "] - TRACE - ";
            break;

        default:
            break;
        }

        return ofs;
    }

    //////////////////////////////
    // Logger Class
    //////////////////////////////

    Logger * Logger::uniqueObjectPtr(nullptr);

    std::mutex Logger::singleInstanceCreationMutex;

    /// <summary>
    /// Gets the unique instance of the singleton <see cref="Logger" /> class.
    /// </summary>
    /// <returns>A pointer to the singleton.</returns>
    Logger * Logger::GetInstance() noexcept
    {
        if (uniqueObjectPtr != nullptr)
            return uniqueObjectPtr;

        try
        {
            CreateInstance(AppConfig::GetApplicationId(),
                AppConfig::GetSettings().common.log.writeToConsole);
        }
        catch (IAppException &appEx)
        {
#ifndef _3FD_PLATFORM_WINRT_UWP
            std::ostringstream oss;
            oss << "The logging facility creation failed with an exception - " << appEx.ToString();
            AttemptConsoleOutput(oss.str());
#endif      // swallow exception (caller might be a destructor)
        }

        return uniqueObjectPtr;
    }

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
                uniqueObjectPtr = dbg_new Logger (id, logToConsole);
        }
        catch(std::system_error &ex)
        {
            throw AppException<std::runtime_error>(StdLibExt::GetDetailsFromSystemError(ex));
        }
        catch(std::bad_alloc &xa)
        {
            throw AppException<std::runtime_error>(xa.what());
        }
    }

    /// <summary>
    /// Shuts down the logger releasing all associated resources.
    /// </summary>
    void Logger::Shutdown()
    {
        try
        {
            std::lock_guard<std::mutex> lock(singleInstanceCreationMutex);

            if (uniqueObjectPtr != nullptr)
            {
                delete uniqueObjectPtr;
                uniqueObjectPtr = nullptr;
            }
        }
        catch (std::system_error &ex)
        {/* DO NOTHING: SWALLOW EXCEPTION
            This method cannot throw an exception because it could have been originally
            invoked by a destructor. When that happens, memory leaks are expected. */
            std::ostringstream oss;
            oss << "System error when shutting down the logger: " << StdLibExt::GetDetailsFromSystemError(ex);
            AttemptConsoleOutput(oss.str());
        }
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="Logger"/> class.
    /// </summary>
    /// <param name="id">The application ID.</param>
    /// <param name="logToConsole">Whether log output should be redirected to the console.
    /// Because a store app does not have an available console, this parameter is ignored.</param>
    Logger::Logger(const string &id, bool logToConsole)
#ifdef _3FD_CONSOLE_AVAILABLE
        : m_fileAccess(logToConsole ? GetConsoleAccess() : GetFileAccess(id))
#else
        : m_fileAccess(GetFileAccess(id))
#endif
#ifdef NDEBUG
        , m_prioThreshold(PRIO_INFORMATION)
#else
        , m_prioThreshold(PRIO_DEBUG)
#endif
    {
        try
        {
            std::thread newThread(&Logger::LogWriterThreadProc, this);
            m_logWriterThread.swap(newThread);
        }
        catch (std::system_error &ex)
        {/* DO NOTHING: SWALLOW EXCEPTION
            Even when the set-up of the logger fails, the application must continue
            to execute, because the logger is merely an auxiliary service. */
            std::ostringstream oss;
            oss << "System error when setting up the logger: " << StdLibExt::GetDetailsFromSystemError(ex);
            AttemptConsoleOutput(oss.str());
            m_fileAccess.reset();
        }
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="Logger"/> class.
    /// </summary>
    Logger::~Logger()
    {
        try
        {
            // Signalizes termination for the message loop
            m_terminationEvent.Signalize();

            if (m_logWriterThread.joinable())
                m_logWriterThread.join();

            _ASSERTE(m_eventsQueue.IsEmpty());

            while (!m_eventsQueue.IsEmpty())
                delete m_eventsQueue.Remove();
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when finalizing the logger: " << StdLibExt::GetDetailsFromSystemError(ex);
            AttemptConsoleOutput(oss.str());
        }
        catch (...)
        {
            AttemptConsoleOutput("Unexpected exception was caught when finalizing the logger");
        }
    }

    /// <summary>
    /// Writes an exception to the log output.
    /// </summary>
    /// <param name="message">The exception to log.</param>
    /// <param name="prio">The priority of the error.</param>
    void Logger::WriteImpl(const IAppException &ex, Priority prio)
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

#if defined _3FD_PLATFORM_WIN32API || defined _3FD_PLATFORM_WINRT_UWP
    /// <summary>
    /// Writes an HRESULT error to the log output.
    /// </summary>
    /// <param name="hr">The HRESULT code.</param>
    /// <param name="message">The main error message.</param>
    /// <param name="function">The name function of the function that returned the error code.</param>
    /// <param name="prio">The priority of the message.</param>
    void Logger::WriteImpl(HRESULT hr, const char *message, const char *function, Priority prio)
    {
        _ASSERTE(FAILED(hr));
        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

#    ifdef _3FD_PLATFORM_WINRT_UWP
        _com_error comErrObj(hr, nullptr);
#   else
        _com_error comErrObj(hr);
#   endif
        std::ostringstream oss;
        oss << "API call " << function << " returned: " << transcoder.to_bytes(comErrObj.ErrorMessage());
        WriteImpl(std::move(message), oss.str(), prio, true);
    }
#endif

    /// <summary>
    /// Writes a message to the log output.
    /// </summary>
    /// <param name="message">The message to log.</param>
    /// <param name="prio">The priority of the message.</param>
    /// <param name="cst">When set to <c>true</c>, append the call stack trace.</param>
    void Logger::WriteImpl(string &&message, Priority prio, bool cst) noexcept
    {
        WriteImpl(std::move(message), string(""), prio, cst);
    }

    /// <summary>
    /// Writes a message and its details to the log output.
    /// </summary>
    /// <param name="what">The reason for the message.</param>
    /// <param name="details">The message details.</param>
    /// <param name="prio">The priority for the message.</param>
    /// <param name="cst">When set to <c>true</c>, append the call stack trace.</param>
    void Logger::WriteImpl(string &&what, string &&details, Priority prio, bool cst) noexcept
    {
        if (!m_fileAccess)
            return;

        try
        {
            using namespace std::chrono;

            auto now = system_clock::to_time_t(system_clock::now());
            auto logEvent = std::make_unique<LogEvent>(now, prio, std::move(what));

#    ifdef ENABLE_3FD_ERR_IMPL_DETAILS
            if (details.empty() == false)
                logEvent->details = std::move(details);
#    endif
#    ifdef ENABLE_3FD_CST
            if (cst && CallStackTracer::IsReady())
                logEvent->trace = CallStackTracer::GetStackReport();
#    endif
            m_eventsQueue.Add(logEvent.release()); // enqueue the request to write this event to the log file
        }
        catch (std::bad_alloc &)
        {
            AttemptConsoleOutput("Failed to write in log output. Could not allocate memory!");
            // swallow exception
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Failed to write in log output. An exception had to be swallowed: " << ex.what();
            AttemptConsoleOutput(oss.str());
            // swallow exception
        }
    }

    /// <summary>
    /// Estimates the room left in the log file for more events.
    /// </summary>
    /// <param name="logFileAccess">Provides access to the log file.</param>
    /// <returns>The estimate, which is positive when there is room left, otherwise, negative.</returns>
    static long EstimateRoomForLogEvents(ILogFileAccess &logFileAccess)
    {
        // Get the file size:
        auto fileSize = logFileAccess.GetFileSize();

#    if defined ENABLE_3FD_CST && defined ENABLE_3FD_ERR_IMPL_DETAILS
        const uint32_t avgLineSize(300);
#    elif defined ENABLE_3FD_CST
        const uint32_t avgLineSize(250);
#    elif defined ENABLE_3FD_ERR_IMPL_DETAILS
        const uint32_t avgLineSize(150);
#    else
        const uint32_t avgLineSize(100);
#    endif
        // Estimate the amount of events for which there is still room left in the log file:
        return static_cast<long> (
            (AppConfig::GetSettings().common.log.sizeLimit * 1024 - fileSize) / avgLineSize
        );
    }

    /// <summary>
    /// The procedure executed by the log writer thread.
    /// </summary>
    void Logger::LogWriterThreadProc()
    {
        try
        {
            bool terminate(false);

            do
            {
                // Wait for queued messages:
                terminate = m_terminationEvent.WaitFor(100);

                // Write the queued messages in the text log file:
                long estimateRoomForLogEvents(0);
                while (!m_eventsQueue.IsEmpty())
                {
                    std::unique_ptr<LogEvent> ev(m_eventsQueue.Remove());

                    // add the main details and message
                    PrepareEventString(m_fileAccess->GetStream(), ev->time, ev->prio) << ev->what;
#   ifdef ENABLE_3FD_ERR_IMPL_DETAILS
                    if (ev->details.empty() == false) // add the details
                        m_fileAccess->GetStream() << " - " << ev->details;
#   endif
#   ifdef ENABLE_3FD_CST
                    if (ev->trace.empty() == false) // add the call stack trace
                        m_fileAccess->GetStream() << _newLine_ _newLine_ "### CALL STACK TRACE ###" _newLine_ << ev->trace;
#   endif
                    m_fileAccess->GetStream() << std::endl << std::flush; // flush the content to the file

                    if (m_fileAccess->HasError())
                        throw AppException<std::runtime_error>("Failed to write in the log output file stream");

                    --estimateRoomForLogEvents;
                }

                // If the log file was supposed to reach its size limit now:
                if (estimateRoomForLogEvents <= 0)
                {
                    // Recalculate estimate for current log file and, if there is no room left, shift to a new one:
                    estimateRoomForLogEvents = EstimateRoomForLogEvents(*m_fileAccess);
                    if (estimateRoomForLogEvents < 0)
                    {
                        m_fileAccess->ShiftToNewLogFile();
                        estimateRoomForLogEvents = EstimateRoomForLogEvents(*m_fileAccess);
                    }
                }

            } while (!terminate);
        }
        catch (...)
        {
            // TO DO: transport exception
        }
    }

}// end of namespace core
}// end of namespace _3fd
