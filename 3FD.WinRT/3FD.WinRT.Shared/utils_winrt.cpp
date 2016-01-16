#include "stdafx.h"
#include "utils_winrt.h"

#include <cassert>

namespace _3fd
{
	namespace utils
	{
		////////////////////////////////////
		// shared_mutex Class
		////////////////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="shared_mutex"/> class.
		/// </summary>
		shared_mutex::shared_mutex()
			: m_curLockType(None)
		{
			InitializeSRWLock(&m_srwLockHandle);
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="shared_mutex"/> class.
		/// </summary>
		shared_mutex::~shared_mutex()
		{
			switch (m_curLockType)
			{
			case _3fd::utils::shared_mutex::Shared:
				unlock_shared();
				break;

			case _3fd::utils::shared_mutex::Exclusive:
				unlock();
				break;

			default:
				break;
			}
		}

		/// <summary>
		/// Acquires a shared lock.
		/// </summary>
		void shared_mutex::lock_shared()
		{
			AcquireSRWLockShared(&m_srwLockHandle);
			m_curLockType = Shared;
		}

		/// <summary>
		/// Releases a shared lock.
		/// </summary>
		void shared_mutex::unlock_shared()
		{
			_ASSERTE(m_curLockType == Shared); // cannot release a lock that was not previously acquired
			ReleaseSRWLockShared(&m_srwLockHandle);
			m_curLockType = None;
		}

		/// <summary>
		/// Acquires an exclusive lock
		/// </summary>
		void shared_mutex::lock()
		{
			AcquireSRWLockExclusive(&m_srwLockHandle);
			m_curLockType = Exclusive;
		}

		/// <summary>
		/// Releases the exclusive lock.
		/// </summary>
		void shared_mutex::unlock()
		{
			_ASSERTE(m_curLockType == Exclusive); // cannot release a lock that was not previously acquired
			ReleaseSRWLockExclusive(&m_srwLockHandle);
			m_curLockType = None;
		}


		////////////////////////////////////
		// WinRTExt Struct
		////////////////////////////////////

		/// <summary>
		/// Determines whether the current thread is the application main STA thread.
		/// </summary>
		/// <returns>
		/// <c>true</c> if the current thread is the application main STA thread or if information
		/// regarding the thread apartment could not be retrieved, otherwise, <c>false</c>.
		/// </returns>
		bool WinRTExt::IsCurrentThreadASTA()
		{
			APTTYPE aptType;
			APTTYPEQUALIFIER aptTypeQualifier;

			auto hres = CoGetApartmentType(&aptType, &aptTypeQualifier);

			_ASSERTE(hres != E_INVALIDARG);

			if (hres != S_OK)
			{
				std::ostringstream oss;
				oss << "COM API error: could not get apartment information from current thread"
					   " - CoGetApartmentType returned "
					<< core::WWAPI::GetHResultLabel(hres);

				throw core::AppException<std::runtime_error>(oss.str());
			}

			return (aptType == APTTYPE_STA || aptType == APTTYPE_MAINSTA);
		}

		// Translates a WinRT exception to a framework exception
		core::AppException<std::runtime_error> WinRTExt::TranslateAsyncWinRTEx(Platform::Exception ^ex)
		{
			std::ostringstream oss;
			oss << "Windows Runtime asynchronous call reported an error: "
				<< core::WWAPI::GetDetailsFromWinRTEx(ex);

			return core::AppException<std::runtime_error>(oss.str());
		}

		/// <summary>
		/// Waits for an asynchronous WinRT action in the application UI STA thread.
		/// </summary>
		/// <param name="asyncAction">The asynchronous WinRT action to wait for.</param>
		void WinRTExt::WaitForAsync(IAsyncAction ^asyncAction)
		{
			try
			{
				/* If callback is completed, just exit. If callback execution
				is not finished, awaiting for completion is allowed as long as
				the current thread is not in app UI STA: */
				if (asyncAction->Status != AsyncStatus::Started || !IsCurrentThreadASTA())
				{
					try
					{
						return concurrency::create_task(asyncAction).get();
					}
					catch (Platform::Exception ^ex)
					{
						throw TranslateAsyncWinRTEx(ex);
					}
				}

				/* Otherwise, await for completion the way done below, which is the easiest
				that works in the app UI STA thread, and transports any eventual exception: */

				std::promise<void> resultPromise;
				auto resultFuture = resultPromise.get_future();

				// Set delegate for completion event:
				asyncAction->Completed = ref new AsyncActionCompletedHandler(
					// Handler for completion:
					[&resultPromise](decltype(asyncAction) action, AsyncStatus status)
					{
						try
						{
							// Get result and fulfill the promise:
							action->GetResults();
							action->Close();
							resultPromise.set_value();
						}
						catch (Platform::Exception ^ex) // transport exception:
						{
							auto appEx = std::make_exception_ptr(TranslateAsyncWinRTEx(ex));
							resultPromise.set_exception(appEx);
						}
					}
				);

				resultFuture.get(); // waits for completion and throws any transported exception
			}
			catch (core::IAppException &)
			{
				throw; // just forward exceptions for errors that have already been handled
			}
			catch (std::future_error &ex)
			{
				std::ostringstream oss;
				oss << "Failed to wait for WinRT asynchronous action: "
					<< core::StdLibExt::GetDetailsFromFutureError(ex);

				throw core::AppException<std::runtime_error>(oss.str());
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic error when waiting for WinRT asynchronous action: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
			catch (Platform::Exception ^ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when preparing to wait for Windows Runtime asynchronous action: "
					<< core::WWAPI::GetDetailsFromWinRTEx(ex);

				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

	} // end of namespace utils
} // end of namespace _3fd
