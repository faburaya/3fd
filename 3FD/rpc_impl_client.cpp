#include "stdafx.h"
#include "rpc_helpers.h"
#include "rpc_impl_util.h"
#include "callstacktracer.h"
#include "logger.h"

#include <codecvt>
#include <memory>
#include <sstream>
#include <NtDsAPI.h>

namespace _3fd
{
    namespace rpc
    {
        /////////////////////////
        // RpcClient Class
        /////////////////////////

        /// <summary>
        /// Initializes a new instance of the <see cref="RpcClient"/> class.
        /// </summary>
        /// <param name="protSeq">The transport to use for RPC.</param>
        /// <param name="objUUID">The UUID of the object in the RPC server. An empty string
        /// is equivalent to a nil UUID, which is valid as long as the endpoint is specified.</param>
        /// <param name="destination">The destination: local RPC requires the machine name,
        /// while for TCP this is the network address (IP or host name).</param>
        /// <param name="authLevel">The authentication level to use.</param>
        /// <param name="authSecurity">The authentication security package to use. Because
        /// local RPC does not support Kerberos, the requirement of such security package will
        /// cause NTLM to be used with mutual authentication (via SPN's registered in AD).
        /// This parameter is ignored if the authentication level specifies that no
        /// authentication takes place.</param>
        /// <param name="serviceClass">A short name used to compose the SPN and identify
        /// the RPC server in the authentication service. This parameter is ignored if the
        /// authentication level specifies that no authentication takes place.</param>
        /// <param name="endpoint">The endpoint: for local RPC is the application or service
        /// name, while for TCP this is the port number. Specifying the endpoint is optional
        /// if the server has registered its bindings with the endpoint mapper.</param>
        /// <param name="impLevel">The level allowed for the RPC server to impersonate
        /// the identity of an authorized client.This parameter is ignored if the
        /// authentication level specifies that no authentication takes place.</param>
        RpcClient::RpcClient(
            ProtocolSequence protSeq,
            const string &objUUID,
            const string &destination,
            AuthenticationLevel authLevel,
            AuthenticationSecurity authSecurity,
            const string &serviceClass,
            const string &endpoint,
            ImpersonationLevel impLevel)
        try
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
                paramObjUuid = (RPC_WSTR)ucs2objUuid.c_str();
            }

            std::wstring protSeqName = transcoder.from_bytes(ToString(protSeq));
            auto paramProtSeq = (RPC_WSTR)protSeqName.c_str();

            std::wstring ucs2Destination = transcoder.from_bytes(destination);
            auto paramNetwAddr = (RPC_WSTR)ucs2Destination.c_str();

            std::wstring ucs2Endpoint;
            RPC_WSTR paramEndpoint;

            if (endpoint.empty() || endpoint == "")
                paramEndpoint = nullptr;
            else
            {
                ucs2Endpoint = transcoder.from_bytes(endpoint);
                paramEndpoint = (RPC_WSTR)ucs2Endpoint.c_str();
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
            ThrowIfError(status, "Failed to create binding handle for RPC client");

            // Release the memory allocated for the binding string:
            status = RpcStringFreeW(&bindingString);
            ThrowIfError(status, "Failed to compose binding string for RPC client");

            if (authLevel == AuthenticationLevel::None)
                return;

            /* Kerberos security package is preferable over NTLM, however, Kerberos is not
            supported in local RPC, and it requires SPN registration, which is only available
            with Microsoft Active Directory services:*/

            std::unique_ptr<wchar_t[]> spnStr;
            DirSvcBinding dirSvcBinding;
            bool useActDirSec(false);

            /* Only try to detect AD...
               + if RPC over TCP and Kerberos is preferable or required, because AD is needed for that
               OR
               + if local RPC and Kerberos was required, because the protocol does not support Kerberos,
                 but there is mutual authentication for NTLM via SPN's */
            if ((protSeq == ProtocolSequence::TCP && authSecurity != AuthenticationSecurity::NTLM)
                || (protSeq == ProtocolSequence::Local && authSecurity == AuthenticationSecurity::RequireKerberos))
            {
                useActDirSec = DetectActiveDirectoryServices(dirSvcBinding, true);

                if(!useActDirSec)
                {
                    core::Logger::Write(
                        "Kerberos security package is not available and NTLM will be used instead",
                        core::Logger::PRIO_NOTICE);
                }
            }

            if (useActDirSec)
            {
                // Prepare parameters to create the server SPN:
                std::wstring ucs2ServiceClass = transcoder.from_bytes(serviceClass);
                auto paramServiceClass = ucs2ServiceClass.c_str();
                auto paramDestination = ucs2Destination.c_str();
                auto paramPort = static_cast<USHORT> (atoi(endpoint.c_str()));

                DWORD spnStrSize(0);

                // First assess the SPN string size...
                auto rc = DsMakeSpnW(
                    paramServiceClass,
                    paramDestination,
                    nullptr,
                    paramPort,
                    nullptr,
                    &spnStrSize,
                    nullptr
                );

                // Check for expected returns when assessing string size:
                if (rc != ERROR_SUCCESS && rc != ERROR_BUFFER_OVERFLOW)
                {
                    std::ostringstream oss;
                    oss << "Could not generate SPN for RPC server - ";
                    core::WWAPI::AppendDWordErrorMessage(rc, "DsMakeSpn", oss);
                    throw core::AppException<std::runtime_error>(oss.str());
                }

                spnStr.reset(new wchar_t[spnStrSize]);

                // ... then get the SPN:
                rc = DsMakeSpnW(
                    paramServiceClass,
                    paramDestination,
                    nullptr,
                    paramPort,
                    nullptr,
                    &spnStrSize,
                    spnStr.get()
                );

                _ASSERTE(rc == ERROR_SUCCESS);

                std::ostringstream oss;
                oss << "RPC client will try to authenticate server \'" << transcoder.to_bytes(spnStr.get()) << '\'';
                core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
            }

            /* Sets the client binding handle's authentication,
            authorization, and security quality-of-service: */

            RPC_SECURITY_QOS_V3_W secQOS = { 0 };
            secQOS.Version = 3;
            secQOS.ImpersonationType = static_cast<unsigned long> (impLevel);

            /* Mutual authentication requires SPN registration,
            hence can only be used when AD is present: */
            if (useActDirSec && authSecurity != AuthenticationSecurity::NTLM)
            {
                secQOS.Capabilities =
                    RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH
                    | RPC_C_QOS_CAPABILITIES_LOCAL_MA_HINT;
            }

            /* Authentication impact on performance on identity tracking
            is negligible unless a remote protocol is in use: */
            if (protSeq == ProtocolSequence::TCP)
                secQOS.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC;
            else
                secQOS.IdentityTracking = RPC_C_QOS_IDENTITY_DYNAMIC;

            /* This client negotiates to use Kerberos if such security package is
            available. When RPC is local, Kerberos is not supported disregarding
            AD availability, so NTLM is used: */

            unsigned long authService;
            if (useActDirSec && protSeq != ProtocolSequence::Local)
            {
                switch (authSecurity)
                {
                case _3fd::rpc::NTLM:
                    authService = RPC_C_AUTHN_WINNT;
                    break;
                case _3fd::rpc::TryKerberos:
                    authService = RPC_C_AUTHN_GSS_NEGOTIATE;
                    break;
                case _3fd::rpc::RequireKerberos:
                    authService = RPC_C_AUTHN_GSS_KERBEROS;
                    break;
                default:
                    _ASSERTE(false); // unsupported option
                    break;
                }   
            }
            else
                authService = RPC_C_AUTHN_WINNT;

            status = RpcBindingSetAuthInfoExW(
                m_bindingHandle,
                (RPC_WSTR)spnStr.get(),
                static_cast<unsigned long> (authLevel),
                authService,
                nullptr, // no explicit credentials, use context
                RPC_C_AUTHZ_DEFAULT,
                reinterpret_cast<RPC_SECURITY_QOS *> (&secQOS)
            );

            ThrowIfError(status, "Failed to set security for binding handle of RPC client");
        }
        catch (core::IAppException &)
        {
            throw; // just forward an exception regarding an error known to have been already handled
        }
        catch (std::exception &ex)
        {
            CALL_STACK_TRACE;
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
            auto status = RpcBindingFree(&m_bindingHandle);
            LogIfError(status,
                "Failed to release resources from binding handle of RPC client",
                core::Logger::Priority::PRIO_CRITICAL);
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
