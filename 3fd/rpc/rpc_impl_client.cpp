#include "pch.h"
#include "rpc_helpers.h"
#include "rpc_impl_util.h"
#include <3fd/core/callstacktracer.h>
#include <3fd/core/configuration.h>
#include <3fd/core/logger.h>
#include <3fd/core/logger.h>
#include <3fd/utils/utils_algorithms.h>
#include <3fd/utils/utils_io.h>

#include <memory>
#include <codecvt>
#include <sstream>
#include <cstdio>
#include <cmath>

namespace _3fd
{
namespace rpc
{
    /////////////////////////
    // RpcClient Class
    /////////////////////////

    /// <summary>
    /// Releases resources of a RPC client binding handle.
    /// </summary>
    /// <param name="bindingHandle">The given RPC binding handle.</param>
    static void HelpFreeBindingHandle(rpc_binding_handle_t *bindingHandle)
    {
        CALL_STACK_TRACE;

        if (*bindingHandle == nullptr)
            return;

        auto status = RpcBindingFree(bindingHandle);

        LogIfError(status,
            "Failed to release resources from binding handle of RPC client",
            core::Logger::Priority::PRIO_CRITICAL);
    }


    /// <summary>
    /// Initializes a new instance of the <see cref="RpcClient"/> class.
    /// This is a basic constructor that sets everything but security options.
    /// </summary>
    /// <param name="protSeq">The transport to use for RPC.</param>
    /// <param name="objUUID">The UUID of the object in the RPC server. An empty string
    /// is equivalent to a nil UUID, which is valid as long as the endpoint is specified.</param>
    /// <param name="destination">The destination: local RPC requires the machine name,
    /// while for TCP this is the network address (IP or host name).</param>
    /// <param name="authnLevel">The authentication level to use.</param>
    /// <param name="endpoint">The endpoint: for local RPC is the application or service
    /// name, while for TCP this is the port number. Specifying the endpoint is optional
    /// if the server has registered its bindings with the endpoint mapper.</param>
    RpcClient::RpcClient(ProtocolSequence protSeq,
                         const string &objUUID,
                         const string &destination,
                         const string &endpoint)
    try :
        m_bindingHandle(nullptr),
        m_isOnHold(false)
    {
        CALL_STACK_TRACE;

        std::ostringstream oss;
        oss << objUUID << '#' << ToString(protSeq) << '@' << destination;

        m_endpoint = oss.str();

        // Prepare text parameters encoded in UCS-2
        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

        std::wstring ucs2objUuid;
        RPC_WSTR paramObjUuid;

        if (objUUID.empty() || objUUID == "")
            paramObjUuid = nullptr;
        else
        {
            ucs2objUuid = transcoder.from_bytes(objUUID);
            paramObjUuid = const_cast<RPC_WSTR> (ucs2objUuid.c_str());
        }

        std::wstring protSeqName = transcoder.from_bytes(ToString(protSeq));
        auto paramProtSeq = const_cast<RPC_WSTR> (protSeqName.c_str());

        std::wstring ucs2Destination = transcoder.from_bytes(destination);
        auto paramNetwAddr = const_cast<RPC_WSTR> (ucs2Destination.c_str());

        std::wstring ucs2Endpoint;
        RPC_WSTR paramEndpoint;

        if (endpoint.empty() || endpoint == "")
            paramEndpoint = nullptr;
        else
        {
            ucs2Endpoint = transcoder.from_bytes(endpoint);
            paramEndpoint = const_cast<RPC_WSTR> (ucs2Endpoint.c_str());
        }

        // Compose the binding string:
        RPC_WSTR bindingString;
        auto status = RpcStringBindingComposeW(
            paramObjUuid,
            paramProtSeq,
            paramNetwAddr,
            paramEndpoint,
            nullptr,
            &bindingString
        );

        ThrowIfError(status, "Failed to compose binding string for RPC client");

        // Create a binding handle from the composed string:
        status = RpcBindingFromStringBindingW(bindingString, &m_bindingHandle);

        // Release the memory allocated for the binding string:
        LogIfError(
            RpcStringFreeW(&bindingString),
            "Failed to release resources of binding string for RPC client",
            core::Logger::Priority::PRIO_CRITICAL
        );

        ThrowIfError(status, "Failed to create binding handle for RPC client");

        // Notify establishment of binding with this protocol sequence:

        oss.str("");
        oss << "RPC client for object " << objUUID << " in " << destination
            << " will use protocol sequence '" << ToString(protSeq) << '\'';

        core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
    }
    catch (core::IAppException &ex)
    {
        CALL_STACK_TRACE;
        HelpFreeBindingHandle(&m_bindingHandle);
        throw core::AppException<std::runtime_error>("Failed to instantiate RPC client", ex);
    }
    catch (std::exception &ex)
    {
        CALL_STACK_TRACE;
        HelpFreeBindingHandle(&m_bindingHandle);
        std::ostringstream oss;
        oss << "Generic failure when instantiating RPC client: " << ex.what();
        throw core::AppException<std::runtime_error>(oss.str());
    }


    /// <summary>
    /// Initializes a new instance of the <see cref="RpcClient"/> class
    /// for security options of Windows SSP packages only (NTLM & Kerberos).
    /// </summary>
    /// <param name="protSeq">The transport to use for RPC.</param>
    /// <param name="objUUID">The UUID of the object in the RPC server. An empty string
    /// is equivalent to a nil UUID, which is valid as long as the endpoint is specified.</param>
    /// <param name="destination">The destination: local RPC requires the machine name,
    /// while for TCP this is the network address (IP or host name).</param>
    /// <param name="authnSecurity">The authentication security package to use. Because
    /// local RPC does not support Kerberos, the requirement of mutual authentication will
    /// cause NTLM to use SPN's registered in AD. This parameter is ignored if the
    /// authentication level specifies that no authentication takes place.</param>
    /// <param name="authnLevel">The authentication level to use.</param>
    /// <param name="impLevel">The level allowed for the RPC server to impersonate
    /// the identity of an authorized client. This parameter is ignored if the
    /// authentication level specifies that no authentication takes place.</param>
    /// <param name="spn">The SPN as registered in AD.</param>
    /// <param name="endpoint">The endpoint: for local RPC is the application or service
    /// name, while for TCP this is the port number. Specifying the endpoint is optional
    /// if the server has registered its bindings with the endpoint mapper.</param>
    RpcClient::RpcClient(ProtocolSequence protSeq,
                         const string &objUUID,
                         const string &destination,
                         AuthenticationSecurity authnSecurity,
                         AuthenticationLevel authnLevel,
                         ImpersonationLevel impLevel,
                         const string &spn,
                         const string &endpoint)
        : RpcClient(protSeq, objUUID, destination, endpoint)
    {
        // Invalid arguments:
        _ASSERTE(
            authnSecurity == AuthenticationSecurity::NTLM
            || authnSecurity == AuthenticationSecurity::TryKerberos
            || authnSecurity == AuthenticationSecurity::RequireMutualAuthn
        );

        CALL_STACK_TRACE;

        try
        {
            if (authnSecurity != AuthenticationSecurity::NTLM
                && authnSecurity != AuthenticationSecurity::TryKerberos
                && authnSecurity != AuthenticationSecurity::RequireMutualAuthn)
            {
                throw core::AppException<std::invalid_argument>(
                    "Invalid argument: the constructor overload was meant to be used only "
                    "with authentication services Microsoft NTLM/Negotiate/Kerberos SSP"
                );
            }

            /* Kerberos security package is preferable over NTLM, however, Kerberos is not
            supported in local RPC, and it requires SPN registration, which is only available
            with Microsoft Active Directory services: */

            DirSvcBinding dirSvcBinding;
            bool useActDirSec(false);

            /* Only try to detect AD...

            + if RPC over TCP and mutual authentication is preferable
            or required, because AD is needed for that
            OR
            + if local RPC and mutual authentication was required,
            because the protocol does not support Kerberos,
            but there is mutual authentication for NTLM using SPN's */

            if ((protSeq == ProtocolSequence::TCP && authnSecurity != AuthenticationSecurity::NTLM)
                || (protSeq == ProtocolSequence::Local && authnSecurity == AuthenticationSecurity::RequireMutualAuthn))
            {
                useActDirSec = DetectActiveDirectoryServices(dirSvcBinding, true);
            }

            std::ostringstream oss;
            std::wstring ucs2spn;
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

            if (useActDirSec)
            {
                if (spn == "")
                {
                    throw core::AppException<std::runtime_error>(
                        "No SPN was provided to RPC client for mutual authentication"
                    );
                }

                ucs2spn = transcoder.from_bytes(spn);

                oss << "RPC client has to authenticate server '" << spn << '\'';
                core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
                oss.str("");
            }
            // AD not available, but mutual authentication was required:
            else if (authnSecurity == AuthenticationSecurity::RequireMutualAuthn)
            {
                oss << "Could not fulfill mutual authentication requirement of "
                       "RPC client for object " << objUUID << " in " << destination
                    << " because Microsoft Active Directory services are not available";

                throw core::AppException<std::runtime_error>(oss.str());
            }

            /* Sets the client binding handle's authentication,
            authorization, and security quality-of-service: */

            RPC_SECURITY_QOS secQOS = { 0 };
            secQOS.Version = 1;
            secQOS.ImpersonationType = static_cast<unsigned long> (impLevel);

            /* Authentication impact on performance on identity tracking
            is negligible unless a remote protocol is in use: */
            if (protSeq == ProtocolSequence::TCP)
                secQOS.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC;
            else
                secQOS.IdentityTracking = RPC_C_QOS_IDENTITY_DYNAMIC;

            /* This client negotiates to use Kerberos if such security package is
            available. When RPC is local, Kerberos is not supported disregarding
            AD availability, so NTLM is used. Regarding the support for mutual
            authentication, both Kerberos and NTLM (only for local RPC) support
            it, but it requires SPN registration, hence can only be used when AD
            is present: */

            unsigned long authnService;

            if (protSeq == ProtocolSequence::Local)
            {
                authnService = RPC_C_AUTHN_WINNT;

                if (useActDirSec && authnSecurity == AuthenticationSecurity::RequireMutualAuthn)
                {
                    secQOS.Capabilities =
                        RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH | RPC_C_QOS_CAPABILITIES_LOCAL_MA_HINT;
                }
            }
            else
            {
                if (useActDirSec)
                {
                    authnService = static_cast<unsigned long> (authnSecurity);

                    if (authnSecurity != AuthenticationSecurity::NTLM)
                        secQOS.Capabilities = RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH;
                }
                else
                    authnService = RPC_C_AUTHN_WINNT;
            }

            auto status = RpcBindingSetAuthInfoExW(
                m_bindingHandle,
                const_cast<RPC_WSTR> (ucs2spn.empty() ? nullptr : ucs2spn.c_str()),
                static_cast<unsigned long> (authnLevel),
                authnService,
                nullptr, // no explicit credentials, use context
                RPC_C_AUTHZ_DEFAULT,
                &secQOS
            );

            ThrowIfError(status, "Failed to set security for binding handle of RPC client");

            oss << "RPC client binding security was set to use "
                << ConvertAuthnSvcOptToString(authnService) << " ";

            AppendSecQosOptsDescription(secQOS, oss);

            oss << ", " << ToString(authnLevel)
                << " and " << ToString(impLevel);

            core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
        }
        catch (core::IAppException &ex)
        {
            HelpFreeBindingHandle(&m_bindingHandle);
            throw core::AppException<std::runtime_error>("Failed to instantiate RPC client", ex);
        }
        catch (std::exception &ex)
        {
            HelpFreeBindingHandle(&m_bindingHandle);
            std::ostringstream oss;
            oss << "Generic failure when creating RPC client: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Initializes a new instance of the <see cref="RpcClient"/> class
    /// for security options of Secure Channel SSP.
    /// </summary>
    /// <param name="protSeq">The transport to use for RPC.</param>
    /// <param name="objUUID">The UUID of the object in the RPC server. An empty string
    /// is equivalent to a nil UUID, which is valid as long as the endpoint is specified.</param>
    /// <param name="destination">The destination: local RPC requires the machine name,
    /// while for TCP this is the network address (IP or host name).</param>
    /// <param name="certInfoX509">Description of the X.509 certificate to use.</param>
    /// <param name="authnLevel">The authentication level to use.</param>
    /// <param name="endpoint">The endpoint: for local RPC is the application or service
    /// name, while for TCP this is the port number. Specifying the endpoint is optional
    /// if the server has registered its bindings with the endpoint mapper.</param>
    /// <remarks>Because Schannel SSP is only compatible with transport
    /// over TCP/IP, that is the implicitly chosen protocol sequence.</remarks>
    RpcClient::RpcClient(const string &objUUID,
                         const string &destination,
                         const CertInfo &certInfoX509,
                         AuthenticationLevel authnLevel,
                         const string &endpoint)
        : RpcClient(ProtocolSequence::TCP, objUUID, destination, endpoint)
    {
        CALL_STACK_TRACE;

        try
        {
            SystemCertificateStore certStore(certInfoX509.storeLocation, certInfoX509.storeName);
            auto certX509 = certStore.FindCertBySubject(certInfoX509.subject);

            if (certX509 == nullptr)
            {
                std::ostringstream oss;
                oss << "Could not get from system store code " << certInfoX509.storeLocation
                    << " the specified X.509 certificate (subject = '" << certInfoX509.subject << "')";

                throw core::AppException<std::runtime_error>(
                    "Certificate for RPC client was not found in store",
                    oss.str()
                );
            }

            auto authnService = static_cast<unsigned long> (AuthenticationSecurity::SecureChannel);

            m_schannelCred.reset(
                dbg_new SChannelCredWrapper(certX509, certInfoX509.strongerSecurity)
            );

            auto status = RpcBindingSetAuthInfoW(
                m_bindingHandle,
                nullptr,
                static_cast<unsigned long> (authnLevel),
                authnService,
                m_schannelCred->GetCredential(),
                RPC_C_AUTHZ_DEFAULT
            );

            ThrowIfError(status, "Failed to set security for binding handle of RPC client");

            std::ostringstream oss;
            oss << "RPC client binding security was set to use "
                << ConvertAuthnSvcOptToString(authnService)
                << " with X.509 certificate (subject = '" << certInfoX509.subject
                << "' in store '" << certInfoX509.storeName
                << "') and " << ToString(authnLevel);

            core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
        }
        catch (core::IAppException &ex)
        {
            HelpFreeBindingHandle(&m_bindingHandle);
            throw core::AppException<std::runtime_error>("Failed to instantiate RPC client", ex);
        }
        catch (std::exception &ex)
        {
            HelpFreeBindingHandle(&m_bindingHandle);
            std::ostringstream oss;
            oss << "Generic failure when creating RPC client: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Finalizes an instance of the <see cref="RpcClient"/> class.
    /// </summary>
    RpcClient::~RpcClient()
    {
        CALL_STACK_TRACE;
        HelpFreeBindingHandle(&m_bindingHandle);
    }


    /// <summary>
    /// Removes the endpoint portion of the server address in the binding handle.
    /// The host remains unchanged in the binding handle. The result is a partially-bound
    /// server binding handle. When the client makes the next remote procedure call using
    /// the reset (partially-bound) binding, the client's run-time library automatically
    /// communicates with the endpoint-mapping service on the specified remote host to obtain
    /// the endpoint of a compatible server from the endpoint-map database. If a compatible
    /// server is located, the RPC run-time library updates the binding with a new endpoint.
    /// If a compatible server is not found, the remote procedure call fails.
    /// </summary>
    void RpcClient::ResetBindings()
    {
        CALL_STACK_TRACE;
        auto status = RpcBindingReset(m_bindingHandle);
        ThrowIfError(status, "Failed to reset binding handle of RPC client");
    }


    /// <summary>
    /// Enumerates the recommended actions for RPC errors.
    /// </summary>
    enum class RpcErrRecommendedAction : uint8_t
    {
        Retry = 0xF0, // just a mask for "retry category"
        SimpleRetry = 0xF2, // for regular issues: just try again after sleeping a constant amount of time
        RetryBackoff = 0xF4, // for resource related issues: try again with sleep intervals in exp back-off
        Reconnect = 0x02, // for connection related issues: attempt reconnection and try RPC again
        Quit = 0x04 // do not insist, just quit
    };


    /// <summary>
    /// Tells by the RPC error status returned by stub call
    /// whether to retry, reconnect & retry or just quit.
    /// </summary>
    /// <param name="errCode">The RPC error code (status).</param>
    static RpcErrRecommendedAction GetRecommendation(RPC_STATUS errCode)
    {
        switch (errCode)
        {
        case RPC_S_CALL_CANCELLED:
        case RPC_S_CALL_FAILED_DNE:
            return RpcErrRecommendedAction::SimpleRetry;

        case RPC_S_SERVER_OUT_OF_MEMORY:
        case RPC_S_SERVER_TOO_BUSY:
            return RpcErrRecommendedAction::RetryBackoff;

        case RPC_S_COMM_FAILURE:
        case RPC_S_NOT_LISTENING:
        case EPT_S_NOT_REGISTERED:
        case RPC_S_SERVER_UNAVAILABLE:
            return RpcErrRecommendedAction::Reconnect;

        default:
            return RpcErrRecommendedAction::Quit;
        }
    }


    // Wraps RPC to isolate SEH code
    static RPC_STATUS CallbackWrapSEH(const std::function<void(rpc_binding_handle_t)> &rpc,
                                      rpc_binding_handle_t bindingHandle)
    {
        RPC_STATUS exCode;

        __try // Because RPC uses SEH, only SEH is allowed in this function
        {
            rpc(bindingHandle);
            return RPC_S_OK;
        }
        // catch with default exception filter:
        __except (RpcExceptionFilter(exCode = RpcExceptionCode()))
        {
            return exCode;
        }
    }


    /// <summary>
    /// Wraps an RPC (with retry loop when appropriate).
    /// </summary>
    /// <param name="tag">A tag for the RPC.</param>
    /// <param name="rpc">The RPC to make.</param>
    /// <param name="bindingHandle">The RPC binding handle.</param>
    /// <returns>The status of the RPC.</returns>
    static RPC_STATUS WrapRpc(const char *tag,
                              const std::function<void(rpc_binding_handle_t)> &rpc,
                              rpc_binding_handle_t bindingHandle)
    {
        int retryCount(0);

        // loop for retries:
        while (true)
        {
            auto status = CallbackWrapSEH(rpc, bindingHandle);

            if (status == RPC_S_OK)
                break;

            static const auto callMaxRetries =
                core::AppConfig::GetSettings().framework.rpc.cliCallMaxRetries;

            auto recomAction = GetRecommendation(status);

            bool mustNotRetry =
                (static_cast<uint8_t> (recomAction)
                 & static_cast<uint8_t> (RpcErrRecommendedAction::Retry)) == 0;

            // must not try again?
            if (mustNotRetry || retryCount == callMaxRetries)
            {
                if (retryCount > 0)
                {
                    std::array<char, 512> bufErrMsg;
                    utils::SerializeTo(bufErrMsg, "RPC call '", tag, "' failed after ", retryCount, " attempt(s)");
                    core::Logger::Write(bufErrMsg.data(), core::Logger::PRIO_INFORMATION);
                }
                
                return status;
            }

            // Wait before retry:

            std::chrono::milliseconds interval;

            if (recomAction == RpcErrRecommendedAction::SimpleRetry)
            {
                static const auto callRetrySleepMs =
                    core::AppConfig::GetSettings().framework.rpc.cliCallRetrySleepMs;

                interval = std::chrono::milliseconds(callRetrySleepMs);
            }
            else // with exponential back-off:
            {
                static const auto callRetryTimeSlotMs =
                    core::AppConfig::GetSettings().framework.rpc.cliCallRetryTimeSlotMs;

                interval = utils::CalcExponentialBackOff(retryCount, std::chrono::milliseconds(callRetryTimeSlotMs));
            }

            std::this_thread::sleep_for(interval);

            ++retryCount;

        }// end of loop

        if (retryCount > 0)
        {
            std::array<char, 512> bufErrMsg;
            utils::SerializeTo(bufErrMsg, "RPC call '", tag, "' had to retry ", retryCount, " time(s) before success");
            core::Logger::Write(bufErrMsg.data(), core::Logger::PRIO_WARNING);
        }

        // quit on success
        return RPC_S_OK;
    }


    /// <summary>
    /// Invokes an RPC with error handling (and re-try/connect loop when appropriate).
    /// </summary>
    /// <param name="tag">A tag for the RPC.</param>
    /// <param name="rpc">The RPC to make.</param>
    void RpcClient::Call(const char *tag, const std::function<void(rpc_binding_handle_t)> &rpc)
    {
        CALL_STACK_TRACE;

        RPC_STATUS status;

        static const auto connectMaxRetries =
            core::AppConfig::GetSettings().framework.rpc.cliSrvConnectMaxRetries;

        int count;

        // loop for reconnection:
        for (count = 0; count <= connectMaxRetries; ++count)
        {
            status = WrapRpc(tag, rpc, GetBindingHandle());

            if (GetRecommendation(status) != RpcErrRecommendedAction::Reconnect)
                break;

            /* Reset the bindings and take a nap. Next RPC will
               attempt to (connect and) bind again to the server: */

            static const auto connRetrySleepSecs =
                core::AppConfig::GetSettings().framework.rpc.cliSrvConnRetrySleepSecs;

            // warn only once about lost connection:
            if (!m_isOnHold.exchange(true))
            {
                std::array<char, 512> bufErrMsg;

                utils::SerializeTo(bufErrMsg,
                    "RPC client stub lost connection to ", m_endpoint,
                    " and will attempt a reconnection every ", connRetrySleepSecs,
                    " seconds up to ", connectMaxRetries, " time(s)");

                core::Logger::Write(bufErrMsg.data(), core::Logger::PRIO_WARNING);
            }

            ResetBindings();

            std::this_thread::sleep_for(
                std::chrono::seconds(connRetrySleepSecs)
            );
        }

        ThrowIfError(status, "Failed to invoke RPC client stub routine", tag);

        // notify only once about reconnection:
        if (m_isOnHold.exchange(false))
        {
            std::array<char, 512> bufErrMsg;

            utils::SerializeTo(bufErrMsg,
                "RPC client stub succesfully reconnected to ",
                m_endpoint, " after ", count, " attempt(s)");

            core::Logger::Write(bufErrMsg.data(), core::Logger::PRIO_WARNING);
        }
    }


    ////////////////////////////////
    // ScopedImpersonation Class
    ////////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="ScopedImpersonation"/> class.
    /// </summary>
    /// <param name="clientBindingHandle">The client binding handle
    /// carrying the identity to impersonate.</param>
    ScopedImpersonation::ScopedImpersonation(rpc_binding_handle_t clientBindingHandle)
        : m_clientBindingHandle(clientBindingHandle)
    {
        CALL_STACK_TRACE;
        auto status = RpcImpersonateClient(clientBindingHandle);
        ThrowIfError(status, "Failed to impersonate indentity of RPC client");
    }


    /// <summary>
    /// Finalizes an instance of the <see cref="ScopedImpersonation"/> class.
    /// </summary>
    ScopedImpersonation::~ScopedImpersonation()
    {
        CALL_STACK_TRACE;
        
        LogIfError(
            RpcRevertToSelfEx(m_clientBindingHandle),
            "Failed to revert impersonation of RPC client",
            core::Logger::PRIO_CRITICAL
        );
    }

}// end of namespace rpc
}// end of namespace _3fd
