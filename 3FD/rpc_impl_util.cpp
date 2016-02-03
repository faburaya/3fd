#include "stdafx.h"
#include "rpc_impl_util.h"
#include "logger.h"

#include <sstream>
#include <codecvt>

namespace _3fd
{
    namespace rpc
    {
        ////////////////////////////
        // RPC Memory Allocation
        ////////////////////////////

        void __RPC_FAR * __RPC_USER midl_user_allocate(size_t len)
        {
            return(malloc(len));
        }

        void __RPC_USER midl_user_free(void __RPC_FAR * ptr)
        {
            free(ptr);
        }

        /////////////////////////
        // Error Helpers
        /////////////////////////

        static
        core::AppException<std::runtime_error>
        CreateException(
            RPC_STATUS errCode,
            const string &message,
            const string &details)
        {
            try
            {
                unsigned short apiMsgUCS2[DCE_C_ERROR_STRING_LEN];

                // Get error message from API:
                auto status = DceErrorInqTextW(errCode, apiMsgUCS2);
                _ASSERTE(status == RPC_S_OK);

                // Transcode from UCS2 to UTF8:
                std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
                string apiMsgUTF8 = transcoder.to_bytes(reinterpret_cast<wchar_t *> (apiMsgUCS2));

                // Assemble the message:
                std::ostringstream oss;
                oss << message << " - System RPC API reported an error: " << apiMsgUTF8;

                // Create the exception:
                if (details.empty() || details == "")
                    return core::AppException<std::runtime_error>(oss.str());
                else
                    return core::AppException<std::runtime_error>(oss.str(), details);
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << message
                    << " - System RPC API reported an error, "
                       "but a generic failure prevented the retrieval of more information: "
                    << ex.what();

                return core::AppException<std::runtime_error>(oss.str());
            }
        }

        // Throws an exception for a system RPC error
        void ThrowIfError(RPC_STATUS status, const char *message)
        {
            if (status == RPC_S_OK)
                return;

            throw CreateException(status, message, "");
        }

        // Throws an exception for a system RPC error
        void ThrowIfError(
            RPC_STATUS status,
            const char *message,
            const string &details)
        {
            if (status == RPC_S_OK)
                return;
            
            throw CreateException(status, message, details);
        }

        // Logs a system RPC error
        void LogIfError(
            RPC_STATUS status,
            const char *message,
            core::Logger::Priority prio) noexcept
        {
            if (status == RPC_S_OK)
                return;

            auto ex = CreateException(status, message, "");
            core::Logger::Write(ex, prio);
        }

        // Logs a system RPC error
        void LogIfError(
            RPC_STATUS status,
            const char *message,
            const string &details,
            core::Logger::Priority prio) noexcept
        {
            if (status == RPC_S_OK)
                return;

            auto ex = CreateException(status, message, details);
            core::Logger::Write(ex, prio);
        }

    }// end of namespace rpc
}// end of namespace _3fd
