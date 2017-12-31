#ifndef UTILS_WINRT_H // header guard
#define UTILS_WINRT_H

#include "base.h"
#include "exceptions.h"
#include "logger.h"

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

    using namespace Windows::Foundation;
    using namespace Windows::Storage;

    /// <summary>
    /// Gathers WinRT API extensions that use C++/CX features.
    /// </summary>
    private ref struct WinRTExt
    {
    private:

        static bool IsCurrentThreadASTA();

        static core::AppException<std::runtime_error> TranslateAsyncWinRTEx(Platform::Exception ^ex);

    internal:

        Windows::Storage::Streams::IRandomAccessStream ^CreateRandomAccessStreamFromBuffer(void *data, ULONG nBytes);

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

        static std::string GetFilePathUtf8(const std::string &fileName, FileLocation where);

        static std::string GetFilePathUtf8(const char *fileName, FileLocation where);

        static std::string GetFilePathUtf8(const wchar_t *fileName, FileLocation where);

        /// <summary>
        /// Awaits an asynchronous WinRT operation (in the app UI STA thread)
        /// that supports progress information.
        /// </summary>
        /// <param name="asyncOp">The asynchronous WinRT operation to wait for.</param>
        /// <returns>The result of the call.</returns>
        template <typename ResultType, typename ProgressType>
        static ResultType WaitForAsync(IAsyncOperationWithProgress<ResultType, ProgressType> ^asyncOp)
        {
            try
            {
                /* If callback is completed, just get the results. If callback execution
                is not finished, awaiting for completion is allowed as long as the current
                thread is not in app UI STA: */
                if (asyncOp->Status != AsyncStatus::Started || !IsCurrentThreadASTA())
                {
                    try
                    {
                        return concurrency::create_task(asyncOp).get();
                    }
                    catch (Platform::Exception ^ex)
                    {
                        throw TranslateAsyncWinRTEx(ex);
                    }
                }

                /* Otherwise, await for completion the way done below, which is the easiest
                that works in the app UI STA thread, and transports any eventual exception: */

                std::promise<ResultType> resultPromise;
                auto resultFuture = resultPromise.get_future();

                // Set delegate for completion event:
                asyncOp->Completed = ref new AsyncOperationWithProgressCompletedHandler<ResultType, ProgressType>(
                    // Handler for completion:
                    [&resultPromise](decltype(asyncOp) op, AsyncStatus status)
                    {
                        try
                        {
                            resultPromise.set_value(op->GetResults()); // get result and fulfill the promise
                            op->Close();
                        }
                        catch (Platform::Exception ^ex) // transport exception:
                        {
                            auto appEx = std::make_exception_ptr(TranslateAsyncWinRTEx(ex));
                            resultPromise.set_exception(appEx);
                        }
                    }
                );

                return resultFuture.get(); // returns the result, or throws a transported exception
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions for errors that have already been handled
            }
            catch (std::future_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to wait for WinRT asynchronous operation: "
                    << core::StdLibExt::GetDetailsFromFutureError(ex);

                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic error when waiting for WinRT asynchronous operation: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (Platform::Exception ^ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when preparing to wait for Windows Runtime asynchronous operation: "
                    << core::WWAPI::GetDetailsFromWinRTEx(ex);

                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Waits for an asynchronous WinRT operation.
        /// </summary>
        /// <param name="asyncOp">The asynchronous WinRT operation to wait for.</param>
        /// <returns>The result of the call.</returns>
        template <typename ResultType> 
        static ResultType WaitForAsync(Windows::Foundation::IAsyncOperation<ResultType> ^asyncOp)
        {
            try
            {
                /* If callback is completed, just get the results. If callback execution
                is not finished, awaiting for completion is allowed as long as the current
                thread is not in app UI STA: */
                if (asyncOp->Status != AsyncStatus::Started || !IsCurrentThreadASTA())
                {
                    try
                    {
                        return concurrency::create_task(asyncOp).get();
                    }
                    catch (Platform::Exception ^ex)
                    {
                        throw TranslateAsyncWinRTEx(ex);
                    }
                }

                /* Otherwise, await for completion the way done below, which is the easiest
                that works in the app UI STA thread, and transports any eventual exception: */

                std::promise<ResultType> resultPromise;
                auto resultFuture = resultPromise.get_future();

                // Set delegate for completion event:
                asyncOp->Completed = ref new AsyncOperationCompletedHandler<ResultType>(
                    // Handler for completion:
                    [&resultPromise](decltype(asyncOp) op, AsyncStatus status)
                    {
                        try
                        {
                            resultPromise.set_value(op->GetResults()); // get result and fulfill the promise
                            op->Close();
                        }
                        catch (Platform::Exception ^ex) // transport exception:
                        {
                            auto appEx = std::make_exception_ptr(TranslateAsyncWinRTEx(ex));
                            resultPromise.set_exception(appEx);
                        }
                    }
                );

                return resultFuture.get(); // returns the result, or throws a transported exception
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions for errors that have already been handled
            }
            catch (std::future_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to wait for WinRT asynchronous operation: "
                    << core::StdLibExt::GetDetailsFromFutureError(ex);

                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic error when waiting for WinRT asynchronous operation: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (Platform::Exception ^ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when preparing to wait for Windows Runtime asynchronous operation: "
                    << core::WWAPI::GetDetailsFromWinRTEx(ex);

                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        static void WaitForAsync(IAsyncAction ^asyncAction);

    };// end of WinRTExt class


    ////////////////////////////////////
    // UWP XAML application utilities
    ////////////////////////////////////

    /// <summary>
    /// Gathers XAML utilities for UWP apps.
    /// </summary>
    struct UwpXaml
    {
        static void Notify(Platform::String ^title,
                           Platform::String ^content,
                           Platform::String ^closeButtonText);

        static void NotifyAndLog(std::exception &ex,
                                 Platform::String ^title,
                                 Platform::String ^closeButtonText,
                                 core::Logger::Priority logEntryPrio = core::Logger::PRIO_ERROR);

        static void NotifyAndLog(Platform::Exception ^ex,
                                 Platform::String ^title,
                                 Platform::String ^closeButtonText,
                                 core::Logger::Priority logEntryPrio = core::Logger::PRIO_ERROR);

        static void NotifyAndLog(core::IAppException &ex,
                                 Platform::String ^title,
                                 Platform::String ^closeButtonText,
                                 core::Logger::Priority logEntryPrio = core::Logger::PRIO_ERROR);


        /// <summary>
        /// Packs some parameters for <see cref="NotifyAndLog"/>.
        /// </summary>
        struct ExNotifAndLogParams
        {
            Platform::StringReference title;
            Platform::StringReference closeButtonText;
            core::Logger::Priority logEntryPrio;
        };


        static bool CheckActionTask(const concurrency::task<void> &task, const ExNotifAndLogParams &exHndParams);


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
        static bool GetTaskRetAndHndEx(const concurrency::task<ResultType> &task,
                                       ResultType &result,
                                       const ExNotifAndLogParams &exHndParams)
        {
            try
            {
                result = task.get();
                return STATUS_OKAY;
            }
            catch (Platform::Exception ^ex)
            {
                NotifyAndLog(ex, exHndParams.title, exHndParams.closeButtonText, exHndParams.logEntryPrio);
            }
            catch (core::IAppException &ex)
            {
                NotifyAndLog(ex, exHndParams.title, exHndParams.closeButtonText, exHndParams.logEntryPrio);
            }
            catch (std::exception &ex)
            {
                NotifyAndLog(ex, exHndParams.title, exHndParams.closeButtonText, exHndParams.logEntryPrio);
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
