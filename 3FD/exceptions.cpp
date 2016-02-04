#include "stdafx.h"
#include "exceptions.h"
#include <codecvt>
#include <sstream>

namespace _3fd
{
	namespace core
	{
		///////////////////////////////////////
		// StdLibExt Class
		///////////////////////////////////////

		/// <summary>
		/// Gets the details from a system error.
		/// </summary>
		/// <param name="ex">The system error.</param>
		/// <returns>A string with a detailed description of the given system error.</returns>
		string StdLibExt::GetDetailsFromSystemError(std::system_error &ex)
		{
			std::ostringstream oss;
			oss << ex.code().category().name() << " / " << ex.code().message();
			return oss.str();
		}

		/// <summary>
		/// Gets the details from a future error.
		/// </summary>
		/// <param name="ex">The future error.</param>
		/// <returns>A string with a detailed description of the given future error.</returns>
		string StdLibExt::GetDetailsFromFutureError(std::future_error &ex)
		{
			std::ostringstream oss;
			oss << ex.code().category().name() << " / " << ex.code().message();
			return oss.str();
		}

#ifdef _WIN32

		///////////////////////////////////////
		// WWAPI Class
		///////////////////////////////////////

		/// <summary>
		/// Get a description label for an HRESULT code.
		/// </summary>
		/// <param name="errCode">The HRESULT code.</param>
		/// <returns>A label with a brief description of the code.</returns>
		string WWAPI::GetHResultLabel(HRESULT errCode)
		{
			std::ostringstream oss;
			oss << "HRESULT error code = 0x" << std::hex << errCode << std::flush;
			return oss.str();
		}

        string WWAPI::GetDWordErrorMessage(DWORD errCode)
        {
            wchar_t *wErrMsgPtr;
            
            auto rc = FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                errCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPWSTR)&wErrMsgPtr, 0,
                nullptr
            );

            if (rc)
            {
                std::ostringstream oss;
                oss << "Error code " << errCode << " (secondary failure prevented retrieval of further details)";
                return oss.str();
            }
            
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            auto message = transcoder.to_bytes(wErrMsgPtr);

            auto hnd = LocalFree(wErrMsgPtr);
            _ASSERTE(hnd == NULL);

            return message;
        }

#	ifdef _3FD_PLATFORM_WINRT // Store Apps only:
		/// <summary>
		/// Gets the details from a WinRT exception.
		/// </summary>
		/// <param name="ex">The exception handle.</param>
		/// <returns>The details about the exception.</returns>
		string WWAPI::GetDetailsFromWinRTEx(Platform::Exception ^ex)
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
			std::ostringstream oss;
			oss << "HRESULT error code = 0x" << std::hex << ex->HResult
				<< " - " << transcoder.to_bytes(ex->Message->Data())
				<< std::flush;

			return oss.str();
		}
#	endif

#endif
    }// end of namespace core
}// end of namespace _3fd
