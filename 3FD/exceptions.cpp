#include "stdafx.h"
#include "exceptions.h"
#include <codecvt>
#include <sstream>

#ifdef _WIN32
#   include <comdef.h>
#endif

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
		/// Get a label for an HRESULT code.
		/// </summary>
		/// <param name="errCode">The HRESULT code.</param>
		/// <returns>A label with the HRESULT code.</returns>
		string WWAPI::GetHResultLabel(HRESULT errCode)
		{
			std::ostringstream oss;
			oss << "HRESULT error code = 0x" << std::hex << errCode << std::flush;
			return oss.str();
		}

        /// <summary>
        /// Get a description for an HRESULT code.
        /// </summary>
        /// <param name="errCode">The HRESULT code.</param>
        /// <returns>A description of the HRESULT code.</returns>
        string WWAPI::GetDetailsFromHResult(HRESULT errCode)
        {
            _ASSERTE(FAILED(errCode));
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            _com_error comErrObj(errCode);
            return transcoder.to_bytes(comErrObj.ErrorMessage());
        }

#   ifdef _3FD_PLATFORM_WIN32API // only for classic desktop apps:
        /// <summary>
        /// Generates an error message for a DWORD error code.
        /// </summary>
        /// <param name="errCode">The error code.</param>
        /// <param name="funcName">Name of the function from Win32 API.</param>
        /// <param name="oss">The string stream where the error message will be appended to.</param>
        void WWAPI::AppendDWordErrorMessage(DWORD errCode,
                                            const char *funcName,
                                            std::ostringstream &oss)
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

            if(funcName != nullptr && funcName[0] != 0)
                oss << funcName << " returned error code " << errCode;
            else
                oss << "code " << errCode;

            if (!rc)
            {
                oss << " (secondary failure prevented retrieval of further details)";
                return;
            }
            
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            oss << ": " << transcoder.to_bytes(wErrMsgPtr);

            auto hnd = LocalFree(wErrMsgPtr);
            _ASSERTE(hnd == NULL);
        }
#   endif

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
