#ifndef RPC_HELPERS_H // header guard
#define RPC_HELPERS_H

#include <3fd/core/exceptions.h>
#include <3fd/core/logger.h>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#define RPC_USE_NATIVE_WCHAR
#include <rpc.h>
#include <AuthZ.h>

namespace _3fd
{
using std::string;

namespace rpc
{
    /// <summary>
    /// Enumerates the possible options for RPC transport.
    /// </summary>
    enum class ProtocolSequence { Local, TCP };

    /// <summary>
    /// Enumerates the possible options for authentication level.
    /// </summary>
    enum class AuthenticationLevel : unsigned long
    {
        Integrity = RPC_C_AUTHN_LEVEL_PKT_INTEGRITY,
        Privacy = RPC_C_AUTHN_LEVEL_PKT_PRIVACY
    };

    /// <summary>
    /// Enumerates the possible options for authentication security (packages).
    /// </summary>
    enum class AuthenticationSecurity : unsigned long
    {
        NTLM = RPC_C_AUTHN_WINNT, // Microsoft NT LAN Manager SSP
        TryKerberos = RPC_C_AUTHN_GSS_NEGOTIATE, // Microsoft Negotiate SSP
        RequireMutualAuthn = RPC_C_AUTHN_GSS_KERBEROS, // Microsoft Kerberos SSP (or NTLM with mutual authentication)
        SecureChannel = RPC_C_AUTHN_GSS_SCHANNEL
    };

    /// <summary>
    /// Enumerates the possible options for impersonation level.
    /// </summary>
    enum class ImpersonationLevel : unsigned long
    {
        Default = RPC_C_IMP_LEVEL_DEFAULT, // automatic
        Identify = RPC_C_IMP_LEVEL_IDENTIFY,
        Impersonate = RPC_C_IMP_LEVEL_IMPERSONATE,
        Delegate = RPC_C_IMP_LEVEL_DELEGATE
    };


    /// <summary>
    /// Holds information that describes a X.509 certificate to use with SCHANNEL SSP.
    /// </summary>
    struct CertInfo
    {
        /// <summary>
        /// The certificate store location (such as CERT_SYSTEM_STORE_CURRENT_USER
        /// or CERT_SYSTEM_STORE_LOCAL_MACHINE) that contains the specified certificate.
        /// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa388136.aspx.
        /// </summary>
        uint32_t storeLocation;

        /// <summary>
        /// The certificate store name (such as "My")
        /// that contains the specified certificate.
        /// </summary>
        string storeName;

        /// <summary>
        /// The certificate subject string.
        /// </summary>
        string subject;

        /// <summary>
        /// Flag to set stronger security options. Allowed cipher suites/algorithms will
        /// be SSL3 and TLS with MAC (weaker ones will be disabled to the detriment of
        /// interoperability) and revocation checks the whole certificate chain.
        /// </summary>
        bool strongerSecurity;

        /// <summary>
        /// Initializes a new instance of the <see cref="CertInfo"/> struct.
        /// </summary>
        /// <param name="p_storeLocation">The certificate store location.</param>
        /// <param name="p_storeName">Name of the certificate store.</param>
        /// <param name="p_subject">The certificate subject.</param>
        /// <param name="p_strongSec">Whether to use stronger security options.</param>
        CertInfo(unsigned long p_storeLocation,
                 const string &p_storeName,
                 const string &p_subject,
                 bool p_strongSec) :
            storeLocation(p_storeLocation),
            storeName(p_storeName),
            subject(p_subject),
            strongerSecurity(p_strongSec)
        {}
    };


    /// <summary>
    /// Holds a definition for a particular RPC interface implementation.
    /// </summary>
    struct RpcSrvObject
    {
        /// <summary>
        /// The UUID of the object, which is an external identifier
        /// exposed to clients. (This is not the interface UUID.)
        /// </summary>
        string uuid;

        /// <summary>
        /// The interface handle defined in the stub generated by MIDL compiler from IDL file.
        /// This handle internally defines the default EPV (when MIDL has been run with '/use_epv').
        /// </summary>
        RPC_IF_HANDLE interfaceHandle;

        /// <summary>
        /// The entry point vector. If this is null, then the default EPV supplied
        /// by the inteface handle is used.
        /// </summary>
        RPC_MGR_EPV *epv;

        RpcSrvObject(const string &p_uuid,
                     RPC_IF_HANDLE p_interfaceHandle,
                     RPC_MGR_EPV *p_epv = nullptr) :
            uuid(p_uuid),
            interfaceHandle(p_interfaceHandle),
            epv(p_epv)
        {}
    };


    class RpcServerImpl;

    /// <summary>
    /// Represents the RPC server that runs inside the application process.
    /// </summary>
    class RpcServer
    {
    private:

        static std::unique_ptr<RpcServerImpl> uniqueObject;

        static std::mutex singletonAccessMutex;

        static void InitializeBaseTemplate(const std::function<void ()> &callback);

        RpcServer() {}

    public:

        RpcServer(const RpcServer &) = delete;

        ~RpcServer() {}

        static void Initialize(
            ProtocolSequence protSeq,
            const string &serviceName
        );

        static void Initialize(
            ProtocolSequence protSeq,
            const string &serviceName,
            AuthenticationLevel authnLevel
        );

        static void Initialize(
            const string &serviceName,
            const CertInfo *certInfoX509,
            AuthenticationLevel authnLevel
        );

        static AuthenticationLevel GetRequiredAuthnLevel();

        static bool Start(const std::vector<RpcSrvObject> &objects);

        static bool Stop();

        static bool Resume();

        static bool Wait();

        static bool Finalize() noexcept;
    };


    class SChannelCredWrapper;

    /// <summary>
    /// Implements an RPC client that provides an explicit binding handle
    /// to use as parameter for client stub code generated by the MIDL compiler.
    /// Client code must derive from this class and use <see cref="RpcClient::Call"/>
    /// to wrap the stub calls with error handling.
    /// </summary>
    class RpcClient
    {
    private:
            
        rpc_binding_handle_t m_bindingHandle;

        string m_endpoint;

        std::unique_ptr<SChannelCredWrapper> m_schannelCred;

        std::atomic<bool> m_isOnHold;

    protected:

        /// <summary>
        /// Gets the binding handle.
        /// </summary>
        /// <returns>The explicit binding handle expected as parameter for RPC.</returns>
        rpc_binding_handle_t GetBindingHandle() const { return m_bindingHandle; }

        void Call(const char *tag, const std::function<void(rpc_binding_handle_t)> &rpc);

    public:

        RpcClient(
            ProtocolSequence protSeq,
            const string &objUUID,
            const string &destination,
            const string &endpoint = ""
        );

        RpcClient(
            ProtocolSequence protSeq,
            const string &objUUID,
            const string &destination,
            AuthenticationSecurity authnSecurity,
            AuthenticationLevel authnLevel = AuthenticationLevel::Integrity,
            ImpersonationLevel impLevel = ImpersonationLevel::Default,
            const string &spn = "",
            const string &endpoint = ""
        );

        RpcClient(
            const string &objUUID,
            const string &destination,
            const CertInfo &certInfoX509,
            AuthenticationLevel authnLevel,
            const string &endpoint = ""
        );

        RpcClient(const RpcClient &) = delete;

        ~RpcClient();

        void ResetBindings();
    };


    /// <summary>
    /// Uses RAII to define a scope where impersonation takes place.
    /// </summary>
    class ScopedImpersonation
    {
    private:

        rpc_binding_handle_t m_clientBindingHandle;

    public:

        ScopedImpersonation(rpc_binding_handle_t clientBindingHandle);

        ScopedImpersonation(const ScopedImpersonation &) = delete;

        ~ScopedImpersonation();
    };

}// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
