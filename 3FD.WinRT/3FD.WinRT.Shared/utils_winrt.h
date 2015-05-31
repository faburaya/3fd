#ifndef UTILS_WINRT_H // header guard
#define UTILS_WINRT_H

#include "base.h"
#include "exceptions.h"

#include <functional>
#include <future>

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

		/// <summary>
		/// Gathers WinRT API extensions that use C++/CX features.
		/// </summary>
		private ref struct WinRTExt
		{
		internal:

			/// <summary>
			/// Waits for an asynchronous WinRT operation that supports progress information.
			/// </summary>
			/// <param name="asyncOp">The asynchronous WinRT operation to wait for.</param>
			/// <returns>The result of the call.</returns>
			template <typename ResultType, typename ProgressType>
			static ResultType WaitForAsync(Windows::Foundation::IAsyncOperationWithProgress<ResultType, ProgressType> ^asyncOp)
			{
				try
				{
					std::promise<ResultType> resultPromise;
					auto resultFuture = resultPromise.get_future();

					// Set delegate for completion event:
					asyncOp->Completed = ref new Windows::Foundation::AsyncOperationWithProgressCompletedHandler<ResultType, ProgressType>(
						// Handler for completion:
						[&resultPromise](decltype(asyncOp) op, Windows::Foundation::AsyncStatus status)
						{
							try
							{
								resultPromise.set_value(op->GetResults()); // get result and fulfill the promise
								op->Close();
							}
							catch (Platform::Exception ^ex) // transport exception:
							{
								op->Close();
								std::ostringstream oss;
								oss << "A Windows Runtime asynchronous call has failed: " << core::WWAPI::GetDetailsFromWinRTEx(ex);
								auto appEx = std::make_exception_ptr(core::AppException<std::runtime_error>(oss.str()));
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
					oss << "Failed to wait for WinRT asynchronous operation: " << core::StdLibExt::GetDetailsFromFutureError(ex);
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
					oss << "Generic failure when preparing to wait for Windows Runtime asynchronous operation: " << core::WWAPI::GetDetailsFromWinRTEx(ex);
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
					std::promise<ResultType> resultPromise;
					auto resultFuture = resultPromise.get_future();

					// Set delegate for completion event:
					asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<ResultType>(
						// Handler for completion:
						[&resultPromise](decltype(asyncOp) op, Windows::Foundation::AsyncStatus status)
						{
							try
							{
								resultPromise.set_value(op->GetResults()); // get result and fulfill the promise
								op->Close();
							}
							catch (Platform::Exception ^ex) // transport exception:
							{
								op->Close();
								std::ostringstream oss;
								oss << "A Windows Runtime asynchronous call has failed: " << core::WWAPI::GetDetailsFromWinRTEx(ex);
								auto appEx = std::make_exception_ptr(core::AppException<std::runtime_error>(oss.str()));
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
					oss << "Failed to wait for WinRT asynchronous operation: " << core::StdLibExt::GetDetailsFromFutureError(ex);
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
					oss << "Generic failure when preparing to wait for Windows Runtime asynchronous operation: " << core::WWAPI::GetDetailsFromWinRTEx(ex);
					throw core::AppException<std::runtime_error>(oss.str());
				}
			}

			static void WaitForAsync(Windows::Foundation::IAsyncAction ^asyncAction);

		};// end of WinRTExt class

	}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
