#ifndef UTILS_WINRT_H // header guard
#define UTILS_WINRT_H

#include "base.h"
#include "exceptions.h"

#include <functional>
#include <future>
#include <string>

namespace _3fd
{
	namespace utils
	{
		/// <summary>
		/// Alternative implementation for std::shared_mutex from C++14 Std library.
		/// </summary>
		class shared_mutex : notcopiable
		{
		private:

			enum LockType : short { None, Shared, Exclusive };

			SRWLOCK m_srwLockHandle;
			LockType m_curLockType;

		public:

			shared_mutex();
			~shared_mutex();

			void lock_shared();
			void unlock_shared();

			void lock();
			void unlock();
		};

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

			/// <summary>
			/// Enumerates some likely location in the sandboxed
			/// storage system of WinRT platform.
			/// </summary>
			enum class FileLocation
			{
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

	}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
