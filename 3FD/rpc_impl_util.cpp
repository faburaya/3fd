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
        /// Converts an enumerated error code for name
        /// handling in AD into a friendly error message.
        /// </summary>
        /// <param name="errorCode">The enumerated error code.</param>
        /// <returns>
        /// A friendly error message corresponding the given error code,
        /// extracted from http://msdn.microsoft.com/en-us/library/windows/desktop/ms676061
        /// </returns>
        const char *ToString(DS_NAME_ERROR error)
        {
            switch (error)
            {
            case DS_NAME_ERROR_RESOLVING:
                return "A generic processing error occurred";

            case DS_NAME_ERROR_NOT_FOUND:
                return "The name cannot be found or the caller "
                       "does not have permission to access the name";

            case DS_NAME_ERROR_NOT_UNIQUE:
                return "The input name is mapped to more than one output "
                       "name or the desired format did not have a single, "
                       "unique value for the object found";

            case DS_NAME_ERROR_NO_MAPPING:
                return "The input name was found, but the associated output "
                       "format cannot be found. This can occur if the object "
                       "does not have all the required attributes";

            case DS_NAME_ERROR_DOMAIN_ONLY:
                return "Domain is determined, but was unable to resolve entire name";

            case DS_NAME_ERROR_NO_SYNTACTICAL_MAPPING:
                return "A syntactical mapping cannot be performed on the client "
                       "without transmitting over the network";

            case DS_NAME_ERROR_TRUST_REFERRAL:
                return "The name is from an external trusted forest";

            default:
                return "Unknown error";
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
                unsigned short apiMsgUCS2[DCE_C_ERROR_STRING_LEN];
                auto status = DceErrorInqTextW(errCode, apiMsgUCS2);

                std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

                if (status == RPC_S_OK)
                    oss << transcoder.to_bytes(reinterpret_cast<wchar_t *> (apiMsgUCS2));
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
