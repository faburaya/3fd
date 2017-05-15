#include "stdafx.h"
#include "utils_winrt.h"
#include "exceptions.h"
#include "logger.h"

#include <cassert>
#include <codecvt>

namespace _3fd
{
namespace utils
{
	////////////////////////
	// WinRTExt Struct
	////////////////////////

	/// <summary>
	/// Gets the path of a specified location of the
	/// sandboxed storage system of WinRT platform.
	/// </summary>
	/// <param name="where">The location.</param>
	/// <returns>The location path, UTF-8 encoded.</returns>
	std::string WinRTExt::GetPathUtf8(FileLocation where)
	{
		using namespace Windows::Storage;

		Platform::String ^path;

		switch (where)
		{
		case WinRTExt::FileLocation::LocalFolder:
			path = ApplicationData::Current->LocalFolder->Path + L"\\";
			break;

		case WinRTExt::FileLocation::TempFolder:
			path = ApplicationData::Current->TemporaryFolder->Path + L"\\";
			break;

		case WinRTExt::FileLocation::RoamingFolder:
			path = ApplicationData::Current->RoamingFolder->Path + L"\\";
			break;

		default:
			_ASSERTE(false);
		}

		std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
		return transcoder.to_bytes(path->Data());
	}

	// Private implementation for 'GetFilePath'
	static std::string GetFilePathUtf8Impl(
		Platform::String ^utf16FileName,
		WinRTExt::FileLocation where,
		std::wstring_convert<std::codecvt_utf8<wchar_t>> &transcoder)
	{
		using namespace Windows::Storage;

		StorageFolder ^folder;

		switch (where)
		{
		case WinRTExt::FileLocation::LocalFolder:
			folder = ApplicationData::Current->LocalFolder;
			break;

		case WinRTExt::FileLocation::TempFolder:
			folder = ApplicationData::Current->TemporaryFolder;
			break;

		case WinRTExt::FileLocation::RoamingFolder:
			folder = ApplicationData::Current->RoamingFolder;
			break;

		default:
			break;
		}

		std::wostringstream woss;
		woss << folder->Path->Data() << L'\\' << utf16FileName->Data();
		return transcoder.to_bytes(woss.str());
	}

	/// <summary>
	/// Gets the path of a file in the specified location
	/// of the sandboxed storage system of WinRT platform.
	/// The files does not need to exist. Only the path is generated.
	/// </summary>
	/// <param name="fileName">Name of the file.</param>
	/// <param name="where">The location where to look for the file.</param>
	/// <returns>The complete path of the file (UTF-8 encoded) in the given location.</returns>
	std::string WinRTExt::GetFilePathUtf8(const std::string &fileName, WinRTExt::FileLocation where)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
		auto utf16FileName = ref new Platform::String(transcoder.from_bytes(fileName).c_str());
		return GetFilePathUtf8Impl(utf16FileName, where, transcoder);
	}

	/// <summary>
	/// Gets the path of a file in the specified location
	/// of the sandboxed storage system of WinRT platform.
	/// If the specified file does not exist, it is created.
	/// </summary>
	/// <param name="fileName">Name of the file.</param>
	/// <param name="where">The location where to look for the file.</param>
	/// <returns>The complete path of the file (UTF-8 encoded) in the given location.</returns>
	std::string WinRTExt::GetFilePathUtf8(const char *fileName, WinRTExt::FileLocation where)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
		auto utf16FileName = ref new Platform::String(transcoder.from_bytes(fileName).c_str());
		return GetFilePathUtf8Impl(utf16FileName, where, transcoder);
	}

	/// <summary>
	/// Gets the path of a file in the specified location
	/// of the sandboxed storage system of WinRT platform.
	/// If the specified file does not exist, it is created.
	/// </summary>
	/// <param name="fileName">Name of the file.</param>
	/// <param name="where">The location where to look for the file.</param>
	/// <returns>The complete path of the file (UTF-8 encoded) in the given location.</returns>
	std::string WinRTExt::GetFilePathUtf8(const wchar_t *fileName, WinRTExt::FileLocation where)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
		Platform::StringReference utf16FileName(fileName);
		return GetFilePathUtf8Impl(utf16FileName, where, transcoder);
	}

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

    ///////////////////////
    // UwpXaml Struct
    ///////////////////////

    /// <summary>
    /// Notifies the user about an error (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="title">The title of the dialog.</param>
    /// <param name="content">The error message.</param>
    /// <param name="closeButtonText">The close button text.</param>
    void UwpXaml::Notify(Platform::String ^title,
                         Platform::String ^content,
                         Platform::String ^closeButtonText)
    {
        auto dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
        dialog->Title = title;
        dialog->SecondaryButtonText = closeButtonText;
        dialog->ShowAsync();
    }

    /// <summary>
    /// Notifies the user about an exception (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="ex">The exception.</param>
    /// <param name="title">The dialog title.</param>
    /// <param name="closeButtonText">The close button text.</param>
    /// <param name="logEntryPrio">The priority for the log entry.</param>
    void UwpXaml::NotifyAndLog(std::exception &ex,
                               Platform::String ^title,
                               Platform::String ^closeButtonText,
                               core::Logger::Priority logEntryPrio)
    {
        std::ostringstream oss;
        oss << "Generic exception: " << ex.what();
        auto utf8content = oss.str();

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto wcontent = ref new Platform::String(
            transcoder.from_bytes(utf8content).c_str()
        );

        Notify(title, wcontent, closeButtonText);

        core::Logger::Write(utf8content, logEntryPrio);
    }

    /// <summary>
    /// Notifies the user about an exception (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="ex">The exception.</param>
    /// <param name="title">The dialog title.</param>
    /// <param name="closeButtonText">The close button text.</param>
    /// <param name="logEntryPrio">The priority for the log entry.</param>
    void UwpXaml::NotifyAndLog(Platform::Exception ^ex,
                               Platform::String ^title,
                               Platform::String ^closeButtonText,
                               core::Logger::Priority logEntryPrio)
    {
        Notify(title, ex->Message, closeButtonText);

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        core::Logger::Write(transcoder.to_bytes(ex->Message->Data()), logEntryPrio);
    }

    /// <summary>
    /// Notifies the user about an exception (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="ex">The exception.</param>
    /// <param name="title">The dialog title.</param>
    /// <param name="closeButtonText">The close button text.</param>
    /// <param name="logEntryPrio">The priority for the log entry.</param>
    void UwpXaml::NotifyAndLog(core::IAppException &ex,
                               Platform::String ^title,
                               Platform::String ^closeButtonText,
                               core::Logger::Priority logEntryPrio)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto content = ref new Platform::String(
            transcoder.from_bytes(ex.ToString()).c_str()
        );

        Notify(title, content, closeButtonText);

        core::Logger::Write(ex.ToString(), logEntryPrio);
    }

} // end of namespace utils
} // end of namespace _3fd
