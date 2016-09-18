#include "stdafx.h"
#include "rpc_helpers.h"
#include "rpc_impl_util.h"
#include "callstacktracer.h"
#include "logger.h"

#include <codecvt>
#include <memory>
#include <sstream>
//#include <NtDsAPI.h>

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
        static void HelpFreeBindingHandle(RPC_BINDING_HANDLE *bindingHandle)
        {
            if (*bindingHandle == nullptr)
                return;

            auto status = RpcBindingFree(bindingHandle);
            LogIfError(status,
                "Failed to release resources from binding handle of RPC client",
                core::Logger::Priority::PRIO_CRITICAL);
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="RpcClient"/> class.
        /// </summary>
        /// <param name="protSeq">The transport to use for RPC.</param>
        /// <param name="objUUID">The UUID of the object in the RPC server. An empty string
        /// is equivalent to a nil UUID, which is valid as long as the endpoint is specified.</param>
        /// <param name="destination">The destination: local RPC requires the machine name,
        /// while for TCP this is the network address (IP or host name).</param>
        /// <param name="endpoint">The endpoint: for local RPC is the application or service
        /// name, while for TCP this is the port number. Specifying the endpoint is optional
        /// if the server has registered its bindings with the endpoint mapper.</param>
        /// <param name="spn">The SPN as registered in AD.</param>
        /// <param name="authnLevel">The authentication level to use.</param>
        /// <param name="authnSecurity">The authentication security package to use. Because
        /// local RPC does not support Kerberos, the requirement of mutual authentication will
        /// cause NTLM to use SPN's registered in AD. This parameter is ignored if the
        /// authentication level specifies that no authentication takes place.</param>
        /// <param name="impLevel">The level allowed for the RPC server to impersonate
        /// the identity of an authorized client. This parameter is ignored if the
        /// authentication level specifies that no authentication takes place.</param>
        RpcClient::RpcClient(
            ProtocolSequence protSeq,
            const string &objUUID,
            const string &destination,
            AuthenticationLevel authnLevel,
            const string &spn,
            AuthenticationSecurity authnSecurity,
            ImpersonationLevel impLevel,
            const string &endpoint)
        try :
            m_bindingHandle(nullptr)
        {
            CALL_STACK_TRACE;

            // Prepare text parameters encoded in UCS-2:
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
            std::ostringstream oss;
            oss << "RPC client for object " << objUUID
                << " in " << destination
                << " will use protocol sequence '" << ToString(protSeq)
                << "' and " << ToString(authnLevel);

            core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
            oss.str("");

            if (authnLevel == AuthenticationLevel::None)
                return;

            /* Kerberos security package is preferable over NTLM, however, Kerberos is not
            supported in local RPC, and it requires SPN registration, which is only available
            with Microsoft Active Directory services: */

            DirSvcBinding dirSvcBinding;
            bool useActDirSec(false);

            /* Only try to detect AD...
               + if RPC over TCP and mutual authentication is preferable or required,
                 because AD is needed for that
               OR
               + if local RPC and mutual authentication was required,
                 because the protocol does not support Kerberos,
                 but there is mutual authentication for NTLM using SPN's */

            if ((protSeq == ProtocolSequence::TCP && authnSecurity != AuthenticationSecurity::NTLM)
                || (protSeq == ProtocolSequence::Local && authnSecurity == AuthenticationSecurity::RequireMutualAuthn))
            {
                useActDirSec = DetectActiveDirectoryServices(dirSvcBinding, true);
            }

            std::wstring ucs2spn;

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
                oss << "Could not fulfill mutual authentication requirement of"
                       " RPC client for object " << objUUID << " in " << destination
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

            if (protSeq != ProtocolSequence::Local)
            {
                authnService = static_cast<unsigned long> (authnSecurity);

                if (useActDirSec && authnSecurity != AuthenticationSecurity::NTLM)
                    secQOS.Capabilities = RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH;
            }
            else
            {
                authnService = RPC_C_AUTHN_WINNT;

                if (useActDirSec && authnSecurity == AuthenticationSecurity::RequireMutualAuthn)
                {
                    secQOS.Capabilities =
                        RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH | RPC_C_QOS_CAPABILITIES_LOCAL_MA_HINT;
                }
            }

            status = RpcBindingSetAuthInfoExW(
                m_bindingHandle,
                const_cast<RPC_WSTR> (ucs2spn.empty() ? nullptr : ucs2spn.c_str()),
                static_cast<unsigned long> (authnLevel),
                authnService,
                nullptr, // no explicit credentials, use context
                RPC_C_AUTHZ_DEFAULT,
                &secQOS
            );

            ThrowIfError(status, "Failed to set security for binding handle of RPC client");
        }
        catch (core::IAppException &)
        {
            CALL_STACK_TRACE;
            HelpFreeBindingHandle(&m_bindingHandle);
            throw; // just forward an exception regarding an error known to have been already handled
        }
        catch (std::exception &ex)
        {
            CALL_STACK_TRACE;
            HelpFreeBindingHandle(&m_bindingHandle);

            std::ostringstream oss;
            oss << "Generic failure when creating RPC client: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
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

        ////////////////////////////////
        // ScopedImpersonation Class
        ////////////////////////////////

        /// <summary>
        /// Initializes a new instance of the <see cref="ScopedImpersonation"/> class.
        /// </summary>
        /// <param name="clientBindingHandle">The client binding handle
        /// carrying the identity to impersonate.</param>
        ScopedImpersonation::ScopedImpersonation(RPC_BINDING_HANDLE clientBindingHandle)
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
            auto status = RpcRevertToSelfEx(m_clientBindingHandle);
            LogIfError(status,
                "Failed to revert impersonation of RPC client",
                core::Logger::PRIO_CRITICAL);
        }

    }// end of namespace rpc
}// end of namespace _3fd
