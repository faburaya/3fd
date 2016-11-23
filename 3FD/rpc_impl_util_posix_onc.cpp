#include "rpc_helpers_posix_onc.h"
#include "callstacktracer.h"
#include "exceptions.h"

#include <sstream>
#include <exception>

namespace _3fd
{
namespace rpc
{
    /// <summary>
    /// Utilizes the status reported by RPC client side calls (clnt_create,
    /// clntraw_create, clnttcp_create or clntudp_create) and throws an
    /// application exception containing the information given in the
    /// parameters, plus the reason provided by the RPC library API to
    /// why the creation of a RPC client handle has failed.
    /// </summary>
    /// <param name="message">The main message to be displayed by the exception.</param>
    /// <param name="function">The name of the RPC library function.</param>
    static void ThrowExForClientCreation(const char *message, const char *function)
    {
        try
        {
            std::ostringstream oss;
            oss << "RPC library (" << function << ")";

            auto errText = clnt_spcreateerror(const_cast<char *> (oss.str().c_str()));

            if (errText == nullptr)
            {
                oss << ": unknown reason! (or wrong usage of API)";
                throw core::AppException<std::logic_error>(message, oss.str());
            }

            oss << ": " << errText;
            throw core::AppException<std::runtime_error>(message, oss.str());
        }
        catch (core::IAppException &)
        {
            throw; // 
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "RPC library (" << function
                << "): a secondary failure prevented retrieval of further details";

            throw core::AppException<std::runtime_error>(message, oss.str());
        }
    }

    /// <summary>
    /// Wraps the creation of a handle for the RPC client.
    /// </summary>
    /// <param name="hostAddr">The name of the remote host where the RPC server is located.</param>
    /// <param name="programId">The ID for the program, as specified in the interface.</param>
    /// <param name="intfVersion">The version specified in the interface.</param>
    /// <param name="protocol">The transport protocol to use.</param>
    /// <return>The newly created RPC client handle.</return>
    static CLIENT *RpcCreateClientHandle(const string &hostAddr,
                                         unsigned long programId,
                                         unsigned long intfVersion,
                                         Protocol protocol)
    {
        CALL_STACK_TRACE;

        char *protLabel;

        switch (protocol)
        {
        case Protocol::TCP:
            protLabel = "tcp";
            break;

        case Protocol::UDP:
            protLabel = "udp";
            break;

        default:
            _ASSERTE(false);
            throw core::AppException<std::logic_error>("Unsupported RPC protocol!");
        }

        auto handle = clnt_create(hostAddr.c_str(), programId, intfVersion, protLabel);

        if (handle == nullptr)
            ThrowExForClientCreation("Failed to create RPC client handle", "clnt_create");

        return handle;
    }
    
    /// <summary>
    /// Utilises the status reported by a RPC client side function
    /// (after clnt_call) and throws an application exception containing
    /// the information given in the parameters, plus what is provided
    /// by the RPC library API.
    /// </summary>
    /// <param name="clientHandle">The handle for the RPC client.</param>
    /// <param name="message">The main message to be displayed by the exception.</param>
    /// <param name="function">The name of the RPC library function.</param>
    void ThrowExForClientCall(CLIENT *clientHandle, const char *message, const char *function)
    {
        try
        {
            std::ostringstream oss;
            oss << "RPC library (" << function << ")";

            auto strErrNo = clnt_sperror(clientHandle, const_cast<char *> (oss.str().c_str()));

            if (strErrNo == nullptr)
            {
                oss << ": could not retrieve details about this error from RPC library API!"
                       " (may be due to wrong usage)";

                throw core::AppException<std::logic_error>(message, oss.str());
            }

            char errText[256];
            strncpy(errText, strErrNo, sizeof errText);
            errText[(sizeof errText) - 1] = 0;

            // Remove the CRLF at the end of the error message:
            auto lastCharPos = strlen(errText) - 1;
            if (errText[lastCharPos] == '\n' && errText[lastCharPos - 1] == '\r')
                errText[lastCharPos] = errText[lastCharPos - 1] = 0;

            oss << ": " << errText;

            throw core::AppException<std::runtime_error>(message, oss.str());
        }
        catch (core::IAppException &)
        {
            throw; // 
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "RPC library (" << function << ") reported an error, "
                   "but a secondary failure prevented retrieval of further details";

            throw core::AppException<std::runtime_error>(message, oss.str());
        }
    }

}// end of namespace rpc
}// end of namespace _3fd
