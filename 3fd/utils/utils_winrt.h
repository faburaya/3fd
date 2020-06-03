#ifndef UTILS_WINRT_H // header guard
#define UTILS_WINRT_H

#include <3fd/core/exceptions.h>
#include <3fd/core/logger.h>

#include <winrt\Windows.Storage.Streams.h>
#include <functional>
#include <future>
#include <string>
#include <memory>

namespace _3fd
{
namespace utils
{
    ////////////////////////////////////
    // Windows Runtime API extensions
    ////////////////////////////////////

    /// <summary>
    /// Gathers WinRT API extensions that use C++/CX features.
    /// </summary>
    class WinRTExt
    {
    private:

        static bool IsCurrentThreadASTA();

    public:

        using IRandomAccessStream = winrt::Windows::Storage::Streams::IRandomAccessStream;

        IRandomAccessStream CreateRandomAccessStreamFromBuffer(void *data, ULONG nBytes);

        /// <summary>
        /// Enumerates some likely location in the sandboxed
        /// storage system of WinRT platform.
        /// </summary>
        enum class FileLocation
        {
            InstallFolder,
            LocalFolder,
            TempFolder,
            RoamingFolder
        };

        static std::string GetPathUtf8(FileLocation where);

        static std::string GetFilePathUtf8(std::string_view fileName, FileLocation where);

        static std::string GetFilePathUtf8(std::wstring_view fileName, FileLocation where);

    };// end of WinRTExt class


    ////////////////////////////////////
    // UWP XAML application utilities
    ////////////////////////////////////

    /// <summary>
    /// Gathers XAML utilities for UWP apps.
    /// </summary>
    struct UwpXaml
    {
        static void Notify(std::string_view title,
                           std::string_view content,
                           std::string_view closeButtonText);

        static void NotifyAndLog(const std::exception &ex,
                                 std::string_view title,
                                 std::string_view closeButtonText,
                                 core::Logger::Priority logEntryPrio = core::Logger::PRIO_ERROR);

        static void NotifyAndLog(const winrt::hresult_error &ex,
                                 std::string_view title,
                                 std::string_view closeButtonText,
                                 core::Logger::Priority logEntryPrio = core::Logger::PRIO_ERROR);

        static void NotifyAndLog(const core::IAppException &ex,
                                 std::string_view title,
                                 std::string_view closeButtonText,
                                 core::Logger::Priority logEntryPrio = core::Logger::PRIO_ERROR);

        static bool CheckActionTask(const concurrency::task<void> &task,
                                    std::string_view title,
                                    std::string_view closeButtonText,
                                    core::Logger::Priority logEntryPrio);

        /// <summary>
        /// Gets the returned value from an asynchronous task, but also handles an
        /// eventual thrown exception by notifying with a dialog and logging the event.
        /// </summary>
        /// <param name="task">The task.</param>
        /// <param name="result">A reference to receive the task return.</param>
        /// <param name="exHndParams">The parameters for notification and logging of the event.</param>
        /// <returns>
        /// <c>STATUS_OKAY</c> if the task finished without throwing an exception, otherwise, <c>STATUS_FAIL</c>.
        /// </returns>
        template <typename ResultType>
        static bool GetTaskRetAndHandleEx(const concurrency::task<ResultType> &task,
                                          ResultType &result,
                                          std::string_view title,
                                          std::string_view closeButtonText,
                                          core::Logger::Priority logEntryPrio)
        {
            try
            {
                result = task.get();
                return STATUS_OKAY;
            }
            catch (winrt::hresult_error &ex)
            {
                NotifyAndLog(ex, title, closeButtonText, logEntryPrio);
            }
            catch (core::IAppException &ex)
            {
                NotifyAndLog(ex, title, closeButtonText, logEntryPrio);
            }
            catch (std::exception &ex)
            {
                NotifyAndLog(ex, title, closeButtonText, logEntryPrio);
            }
            catch (...)
            {
                _ASSERTE(false); // extend implementation to handle more exception types!
            }

            return STATUS_FAIL;
        }

    };// end of struct UwpXaml

}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
