#include "stdafx.h"
#include "exceptions.h"
#include <codecvt>
#include <sstream>

#if defined _3FD_PLATFORM_WIN32API || defined _3FD_PLATFORM_WINRT
#   include <comdef.h>
#   include <array>
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
            oss << "HRESULT error code = 0x" << std::hex << errCode;
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

#        ifdef _3FD_PLATFORM_WINRT
            _com_error comErrObj(errCode, nullptr);
#       else
            _com_error comErrObj(errCode);
#       endif
            auto text = transcoder.to_bytes(comErrObj.ErrorMessage());

            while (text.back() == '\n' || text.back() == '\r')
                text.pop_back();

            return std::move(text);
        }

        /// <summary>
        /// Raises an exception for an HRESULT error code.
        /// </summary>
        /// <param name="errCode">The HRESULT code.</param>
        /// <param name="message">The main error message.</param>
        /// <param name="function">The name function of the function that returned the error code.</param>
        void WWAPI::RaiseHResultException(HRESULT errCode, const char *message, const char *function)
        {
            std::ostringstream oss;
            oss << message << " - API call " << function
                << " returned HRESULT error code 0x" << std::hex << errCode
                << ": " << GetDetailsFromHResult(errCode);

            throw HResultException(errCode, oss.str());
        }

        /// <summary>
        /// Raises an exception for a COM error.
        /// </summary>
        /// <param name="errCode">The COM exception.</param>
        /// <param name="message">The main error message.</param>
        void WWAPI::RaiseHResultException(const _com_error &ex, const char *message)
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            std::ostringstream oss;
            oss << message << " - HRESULT error code 0x"
                << std::hex << ex.Error() << ": "
                << transcoder.to_bytes(ex.ErrorMessage());

            auto text = oss.str();
            while (text.back() == '\n' || text.back() == '\r')
                text.pop_back();

            throw HResultException(ex.Error(), std::move(text));
        }

#   ifdef _3FD_PLATFORM_WIN32API // only for classic desktop apps:
        /// <summary>
        /// Generates an error message for a DWORD error code.
        /// </summary>
        /// <param name="errCode">The error code.</param>
        /// <param name="funcName">Name of the function from Win32 API.</param>
        /// <param name="oss">The string stream where the error message will be appended to.</param>
        /// <param name="dlibHandle">Optional: a handle for a system library
        /// containing further information about the API in error.</param>
        void WWAPI::AppendDWordErrorMessage(DWORD errCode,
                                            const char *funcName,
                                            std::ostringstream &oss,
                                            HMODULE dlibHandle)
        {
            wchar_t *wErrMsgPtr;

            DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

            if (dlibHandle != nullptr)
                flags |= FORMAT_MESSAGE_FROM_HMODULE;
            
            auto rc = FormatMessageW(
                flags,
                dlibHandle,
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

#    ifdef _3FD_PLATFORM_WINRT // Store Apps only:
        /// <summary>
        /// Gets the details from a WinRT exception.
        /// </summary>
        /// <param name="ex">The exception handle.</param>
        /// <returns>The details about the exception.</returns>
        string WWAPI::GetDetailsFromWinRTEx(Platform::Exception ^ex)
        {
            std::wostringstream woss;
            woss << L"HRESULT error code 0x" << std::hex << ex->HResult << L": ";

            std::array<wchar_t, 256> buffer;
            wcsncpy(buffer.data(), ex->Message->Data(), buffer.size());
            buffer[buffer.size() - 1] = L'\0';

            auto token = wcstok(buffer.data(), L"\r\n");
            while (true)
            {
                woss << token;

                token = wcstok(nullptr, L"\r\n");
                if (token != nullptr)
                    woss << L" - ";
                else
                    break;
            }

            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            return transcoder.to_bytes(woss.str());
        }
#    endif

#endif
    }// end of namespace core
}// end of namespace _3fd
