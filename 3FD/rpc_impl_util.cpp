#include "stdafx.h"
#include "preprocessing.h"
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

        ////////////////////////////
        // Translation of Types
        ////////////////////////////

        /// <summary>
        /// Converts an enumerated protocol sequence option into
        /// the corresponding string expected by the RPC API.
        /// </summary>
        /// <param name="protSeq">The protol sequence to convert.</param>
        /// <returns>A string with the name of the protocol sequence
        /// as expected by the system RPC API.</returns>
        const char *ToString(ProtocolSequence protSeq)
        {
            // What protocol sequence?
            switch (protSeq)
            {
            case ProtocolSequence::Local:
                return "ncalrpc";
                
            case ProtocolSequence::TCP:
                return "ncacn_ip_tcp";
                
            default:
                _ASSERTE(false);
                return "UNSUPPORTED";
            }
        }

        /// <summary>
        /// Converts an enumerated authentication level option
        /// into a descriptive label for it.
        /// </summary>
        /// <param name="protSeq">The authentication level to convert.</param>
        /// <returns>A string with a label for the authentication.</returns>
        const char *ToString(AuthenticationLevel authnLevel)
        {
            switch (authnLevel)
            {
            case AuthenticationLevel::None:
                return "no authentication";

            case AuthenticationLevel::Integrity:
                return R"(authentication level "integrity")";

            case AuthenticationLevel::Privacy:
                return R"(authentication level "privacy")";

            default:
                _ASSERTE(false);
                return "UNSUPPORTED";
            }
        }

        //////////////////////
        // UUID_VECTOR Fix
        //////////////////////

        VectorOfUuids::~VectorOfUuids()
        {
            for (auto ptr : m_ptrs2Uuids)
                delete ptr;
        }

        void VectorOfUuids::Add(const UUID &uuid)
        {
            if (m_ptrs2Uuids.size() < UUID_VECTOR_MAX_SIZE)
                m_ptrs2Uuids.push_back(new UUID(uuid));
            else
            {
                std::ostringstream oss;
                oss << "Could not copy object UUID because "
                       "the amount of implementations for the RPC interface "
                       "exceeded a practical limit of " << UUID_VECTOR_MAX_SIZE;

                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        UUID_VECTOR * VectorOfUuids::CopyTo(UuidVectorFix &vec) noexcept
        {
            _ASSERTE(m_ptrs2Uuids.size() <= UUID_VECTOR_MAX_SIZE);

            for (unsigned long idx = 0; idx < m_ptrs2Uuids.size(); ++idx)
                vec.data[idx] = m_ptrs2Uuids[idx];

            vec.size = m_ptrs2Uuids.size();
            return reinterpret_cast<UUID_VECTOR *> (&vec);
        }

        /// <summary>
        /// Detects the presence of Microsoft Active Directory services.
        /// </summary>
        /// <param name="dirSvcBinding">A wrapper for the AD binding handle.</param>
        /// <param name="isClient">Set to <c>true</c> if this call is made by an RPC client.</param>
        /// <returns>
        /// Whether AD services is present.
        /// </returns>
        bool DetectActiveDirectoryServices(DirSvcBinding &dirSvcBinding, bool isClient)
        {
            CALL_STACK_TRACE;

            std::ostringstream oss;

            // Attempt to bind to a domain in Active Directory:
            auto rc = DsBindW(nullptr, nullptr, &dirSvcBinding.handle);

            if (rc == ERROR_SUCCESS)
            {
                oss << "Microsoft Active Directory is available and RPC "
                    << (isClient ? "client " : "server ")
                    << "will attempt to use Kerberos authentication service";

                core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
                return true;
            }
            else if (rc == ERROR_NO_SUCH_DOMAIN)
            {
                oss << "Because of a failure to bind to the global catalog server, the RPC "
                    << (isClient ? "client " : "server ")
                    << "will assume Microsoft Active Directory unavailable";

                core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
                return false;
            }
            else
            {
                oss << "Could not bind to a domain controller - ";
                core::WWAPI::AppendDWordErrorMessage(rc, "DsBind", oss);
                throw core::AppException<std::runtime_error>(oss.str());
            }
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
                // Assemble the message:
                std::ostringstream oss;
                oss << message << " - System RPC API reported an error: ";

                // Get error message from API:
                wchar_t apiMsgUCS2[DCE_C_ERROR_STRING_LEN];
                auto status = DceErrorInqTextW(errCode, apiMsgUCS2);

                std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

                if (status == RPC_S_OK)
                {
                    // Remove the CRLF at the end of the error message:
                    auto lastCharPos = wcslen(apiMsgUCS2) - 1;
                    if (apiMsgUCS2[lastCharPos] == '\n' && apiMsgUCS2[lastCharPos - 1] == '\r')
                        apiMsgUCS2[lastCharPos] = apiMsgUCS2[lastCharPos - 1] = 0;

                    oss << transcoder.to_bytes(apiMsgUCS2);
                }
                else
                    core::WWAPI::AppendDWordErrorMessage(status, nullptr, oss);

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
        
        /// <summary>
        /// Throws an exception for a system RPC error.
        /// </summary>
        /// <param name="status">The status returned by the RPC API.</param>
        /// <param name="message">The main message for the error.</param>
        void ThrowIfError(RPC_STATUS status, const char *message)
        {
            if (status == RPC_S_OK)
                return;

            throw CreateException(status, message, "");
        }

        /// <summary>
        /// Throws an exception for a system RPC error.
        /// </summary>
        /// <param name="status">The status returned by the RPC API.</param>
        /// <param name="message">The main message for the error.</param>
        /// <param name="details">The details for the error.</param>
        void ThrowIfError(
            RPC_STATUS status,
            const char *message,
            const string &details)
        {
            if (status == RPC_S_OK)
                return;
            
            throw CreateException(status, message, details);
        }

        /// <summary>
        /// Logs a system RPC error.
        /// </summary>
        /// <param name="status">The status returned by the RPC API.</param>
        /// <param name="message">The main message for the error.</param>
        /// <param name="prio">The priority for event to be logged.</param>
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

        /// <summary>
        /// Logs a system RPC error.
        /// </summary>
        /// <param name="status">The status returned by the RPC API.</param>
        /// <param name="message">The main message for the error.</param>
        /// <param name="details">The details for the error.</param>
        /// <param name="prio">The priority for event to be logged.</param>
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
