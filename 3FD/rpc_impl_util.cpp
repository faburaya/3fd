#include "stdafx.h"
#include "preprocessing.h"
#include "rpc_impl_util.h"
#include "logger.h"

#include <string>
#include <sstream>

#ifdef _3FD_MICROSOFT_RPC
#   include <codecvt>
#else
#   include <dce/dce_error.h>
#endif

namespace _3fd
{
namespace rpc
{
    using std::string;

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
#   ifdef _3FD_MICROSOFT_RPC
        case ProtocolSequence::Local:
            return "ncalrpc";
#   endif
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
    /// <param name="authnLevel">The authentication level option to convert.</param>
    /// <returns>A string with a label for the authentication level.</returns>
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
            return "UNRECOGNIZED AUTHENTICATION LEVEL";
        }
    }

    /// <summary>
    /// Converts an enumerated impersonation level option
    /// into a descriptive label for it.
    /// </summary>
    /// <param name="impersonationLevel">The impersonation level option to convert.</param>
    /// <returns>A string with a label for the impersonation level.</returns>
    const char *ToString(ImpersonationLevel impersonationLevel)
    {
        switch (impersonationLevel)
        {
        case ImpersonationLevel::Default:
            return R"(impersonation level "default")";

        case ImpersonationLevel::Identify:
            return R"(impersonation level "identify")";

        case ImpersonationLevel::Impersonate:
            return R"(impersonation level "impersonate")";

        case ImpersonationLevel::Delegate:
            return R"(impersonation level "delegate")";

        default:
            _ASSERTE(false);
            return "UNRECOGNIZED IMPERSONATION LEVEL";
        }
    }

    /// <summary>
    /// Converts an authentication service option from
    /// Win32 API into a descriptive label for it.
    /// </summary>
    /// <param name="impersonationLevel">The authentication service option to convert.</param>
    /// <returns>A string with a label for the authentication service.</returns>
    const char *ConvertAuthnSvcOptToString(unsigned long authnService)
    {
        switch (authnService)
        {
#   ifdef _3FD_MICROSOFT_RPC
        case RPC_C_AUTHN_WINNT:
            return R"(authentication service "Microsoft NTLM SSP")";

        case RPC_C_AUTHN_GSS_NEGOTIATE:
            return R"(authentication service "Microsoft Negotiate SSP")";

        case RPC_C_AUTHN_GSS_KERBEROS:
            return R"(authentication service "Microsoft Kerberos SSP")";
#   endif
        case RPC_IMPL_SWITCH(RPC_C_AUTHN_GSS_SCHANNEL, rpc_c_authn_schannel):
            return RPC_IMPL_SWITCH(
                R"(authentication service "Microsoft Schannel SSP")",
                R"(authentication service "Secure Channel")"
            );

        default:
            _ASSERTE(false);
            return "UNRECOGNIZED AUTHENTICATION SERVICE";
        }
    }

#   ifdef _3FD_MICROSOFT_RPC

    /// <summary>
    /// Gets a structure with security QOS options for Microsoft RPC,
    /// generates a text description for it and append to output stream.
    /// </summary>
    /// <param name="secQOS">An structure used by Microsoft RPC implementation to hold security QOS options.</param>
    /// <param name="oss">The output string stream.</param>
    void AppendSecQosOptsDescription(const RPC_SECURITY_QOS &secQOS, std::ostringstream &oss)
    {
        if (secQOS.Capabilities & RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH != 0)
            oss << "with mutual authentication, ";
        else
            oss << "with NO mutual authentication, ";

        switch (secQOS.IdentityTracking)
        {
        case RPC_C_QOS_IDENTITY_STATIC:
            oss << "static identity tracking";
            break;

        case RPC_C_QOS_IDENTITY_DYNAMIC:
            oss << "dynamic identity tracking";
            break;

        default:
            _ASSERTE(false);
            oss << "UNRECOGNIZED ID TRACKING MODE";
            break;
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
    
    //////////////////////////////////
    // SystemCertificateStore Class
    //////////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="SystemCertificateStore"/> class.
    /// </summary>
    /// <param name="registryLocation">
    /// The certificate store location (such as CERT_SYSTEM_STORE_CURRENT_USER
    /// or CERT_SYSTEM_STORE_LOCAL_MACHINE) that contains the specified certificate.
    /// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa388136.aspx.
    /// </param>
    /// <param name="storeName">The certificate store name
    /// (such as "My") that contains the specified certificate.</param>
    SystemCertificateStore::SystemCertificateStore(DWORD registryLocation, const string &storeName)
        : m_certStoreHandle(nullptr)
    {
        CALL_STACK_TRACE;

        try
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

            m_certStoreHandle = CertOpenStore(
                CERT_STORE_PROV_SYSTEM_W,
                X509_ASN_ENCODING,
                0,
                registryLocation,
                transcoder.from_bytes(storeName).c_str()
            );

            if (m_certStoreHandle == nullptr)
            {
                std::ostringstream oss;
                oss << "Failed to open system certificate store - ";
                core::WWAPI::AppendDWordErrorMessage(GetLastError(), "CertOpenStore", oss);
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }
        catch (core::IAppException &)
        {
            // just forward an exception regarding an error known to have been already handled
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when opening system certificate store: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="SystemCertificateStore"/> class.
    /// </summary>
    SystemCertificateStore::~SystemCertificateStore()
    {
        try
        {
            if (CertCloseStore(m_certStoreHandle, 0) == FALSE)
            {
                CALL_STACK_TRACE;
                std::ostringstream oss;
                oss << "Failed to close system certificate store - ";
                core::WWAPI::AppendDWordErrorMessage(GetLastError(), "CertCloseStore", oss);
                core::Logger::Write(oss.str(), core::Logger::PRIO_ERROR, true);
            }
        }
        catch (std::exception &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Generic failure when releasing resources "
                   "of system certificate store: " << ex.what();

            core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL, true);
        }
    }

    /// <summary>
    /// Finds and retrieves from the system store a
    /// X.509 certificate with a given subject.
    /// </summary>
    /// <param name="certSubject">The specified subject string
    /// in the certificate to look for.</param>
    /// <return>The first matching certificate when found,
    /// otherwise, <c>nullptr</c>.</return>
    PCCERT_CONTEXT SystemCertificateStore::FindCertBySubject(const string &certSubject) const
    {
        CALL_STACK_TRACE;

        try
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

            auto certCtxtHandle = CertFindCertificateInStore(
                m_certStoreHandle,
                X509_ASN_ENCODING,
                0,
                CERT_FIND_SUBJECT_STR_W,
                transcoder.from_bytes(certSubject).c_str(),
                nullptr
            );

            if (certCtxtHandle == nullptr
                && GetLastError() != CRYPT_E_NOT_FOUND)
            {
                std::ostringstream oss;
                oss << "Failed to find X.509 certificate in store - ";
                core::WWAPI::AppendDWordErrorMessage(GetLastError(), "CertFindCertificateInStore", oss);
                throw core::AppException<std::runtime_error>(oss.str());
            }

            return certCtxtHandle;
        }
        catch (core::IAppException &)
        {
            // just forward an exception regarding an error known to have been already handled
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when searching for certificate in store: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

    //////////////////////////////////
    // SChannelCredWrapper Class
    //////////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="SChannelCredWrapper"/> class.
    /// Instances initialized by this constructor represent credentials for the RPC client.
    /// </summary>
    /// <param name="certCtxtHandle">Handle to the certificate context.</param>
    /// <param name="strongerSec">
    /// Flag to set stronger security options. Allowed cipher suites/algorithms will
    /// be SSL3 and TLS with MAC (weaker ones will be disabled to the detriment of
    /// interoperability) and revocation checks the whole certificate chain.
    /// </param>
    SChannelCredWrapper::SChannelCredWrapper(PCCERT_CONTEXT certCtxtHandle, bool strongerSec)
    try :
        m_credStructure{ 0 }
    {
        m_credStructure.dwVersion = SCHANNEL_CRED_VERSION;
        m_credStructure.cCreds = 1;
        m_credStructure.paCred = new PCCERT_CONTEXT[1]{ certCtxtHandle };

        if (strongerSec)
        {
            m_credStructure.grbitEnabledProtocols =
                SP_PROT_SSL3_CLIENT | SP_PROT_TLS1_X_CLIENT | SP_PROT_DTLS1_X_CLIENT;

            m_credStructure.dwMinimumCipherStrength = -1;
            m_credStructure.dwMaximumCipherStrength = -1;

            m_credStructure.dwFlags =
                SCH_CRED_REVOCATION_CHECK_CHAIN | SCH_USE_STRONG_CRYPTO;
        }
    }
    catch (std::exception &ex)
    {
        std::ostringstream oss;
        oss << "Generic failure when instantiating RPC client "
               "credential for secure channel: " << ex.what();

        throw core::AppException<std::runtime_error>(oss.str());
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="SChannelCredWrapper"/> class.
    /// Instances initialized by this constructor represent credentials for the RPC server.
    /// </summary>
    /// <param name="certStoreHandle">Handle to the certificate store.</param>
    /// <param name="certCtxtHandle">Handle to the certificate context.</param>
    /// <param name="strongerSec">
    /// Flag to set stronger security options. Allowed cipher suites/algorithms will
    /// be SSL3 and TLS with MAC (weaker ones will be disabled to the detriment of
    /// interoperability) and revocation will check the whole certificate chain.
    ///</param>
    SChannelCredWrapper::SChannelCredWrapper(HCERTSTORE certStoreHandle,
                                             PCCERT_CONTEXT certCtxtHandle,
                                             bool strongerSec)
    try :
        m_credStructure{ 0 }
    {
        m_credStructure.dwVersion = SCHANNEL_CRED_VERSION;
        m_credStructure.cCreds = 1;
        m_credStructure.paCred = new PCCERT_CONTEXT[1]{ certCtxtHandle };
        m_credStructure.hRootStore = certStoreHandle;

        if (strongerSec)
        {
            m_credStructure.grbitEnabledProtocols =
                SP_PROT_SSL3_SERVER | SP_PROT_TLS1_X_SERVER | SP_PROT_DTLS1_X_SERVER;

            m_credStructure.dwMinimumCipherStrength = -1;
            m_credStructure.dwMaximumCipherStrength = -1;

            m_credStructure.dwFlags =
                SCH_CRED_REVOCATION_CHECK_CHAIN | SCH_USE_STRONG_CRYPTO;
        }
    }
    catch (std::exception &ex)
    {
        std::ostringstream oss;
        oss << "Generic failure when instantiating RPC server "
               "credential for secure channel: " << ex.what();

        throw core::AppException<std::runtime_error>(oss.str());
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="SChannelCredWrapper"/> class.
    /// </summary>
    SChannelCredWrapper::~SChannelCredWrapper()
    {
        CertFreeCertificateContext(m_credStructure.paCred[0]);
        delete m_credStructure.paCred;
    }

#   else // Not Windows platform: DCE RPC



#   endif

    /////////////////////////
    // Error Helpers
    /////////////////////////

    static
    core::AppException<std::runtime_error>
    CreateException(
        rpc_status_t errCode,
        const string &message,
        const string &details)
    {
        const char *labelRpcImpl = RPC_IMPL_SWITCH("RPC runtime", "DCE RPC library");

        try
        {
            // Assemble the message:
            std::ostringstream oss;
            oss << message << " - " << labelRpcImpl << " reported an error: ";

            RPC_IMPL_SWITCH(wchar_t, char) errorMessage[
                RPC_IMPL_SWITCH(DCE_C_ERROR_STRING_LEN, dce_c_error_string_len)
            ];

            // Get error message from API:
            RPC_IMPL_SWITCH(rpc_status_t, int) status;
            dce_error_inq_text(errCode, errorMessage, &status);

            if (status == RPC_S_OK)
            {
                RPC_MS_ONLY(std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder);

                // Remove the CRLF at the end of the error message:
                auto lastCharPos = rpc_strlen(errorMessage) - 1;
                if (errorMessage[lastCharPos] == '\n' && errorMessage[lastCharPos - 1] == '\r')
                    errorMessage[lastCharPos] = errorMessage[lastCharPos - 1] = 0;
                
                oss << RPC_IMPL_SWITCH(transcoder.to_bytes(errorMessage), errorMessage);
            }
#   ifdef _3FD_MICROSOFT_RPC
            else
                core::WWAPI::AppendDWordErrorMessage(status, nullptr, oss);
#   endif
            // Create the exception:
            if (details.empty() || details == "")
                return core::AppException<std::runtime_error>(oss.str());
            else
                return core::AppException<std::runtime_error>(oss.str(), details);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << message << " - " << labelRpcImpl << " reported an error, but a generic "
                   "failure prevented the retrieval of more information: " << ex.what();

            return core::AppException<std::runtime_error>(oss.str());
        }
    }
        
    /// <summary>
    /// Throws an exception for a RPC runtime error.
    /// </summary>
    /// <param name="status">The status returned by the RPC API.</param>
    /// <param name="message">The main message for the error.</param>
    void ThrowIfError(rpc_status_t status, const char *message)
    {
        if (status == RPC_S_OK)
            return;

        throw CreateException(status, message, "");
    }

    /// <summary>
    /// Throws an exception for a RPC error.
    /// </summary>
    /// <param name="status">The status returned by the RPC API.</param>
    /// <param name="message">The main message for the error.</param>
    /// <param name="details">The details for the error.</param>
    void ThrowIfError(
        rpc_status_t status,
        const char *message,
        const string &details)
    {
        if (status == RPC_S_OK)
            return;
            
        throw CreateException(status, message, details);
    }

    /// <summary>
    /// Logs a RPC error.
    /// </summary>
    /// <param name="status">The status returned by the RPC API.</param>
    /// <param name="message">The main message for the error.</param>
    /// <param name="prio">The priority for event to be logged.</param>
    void LogIfError(
        rpc_status_t status,
        const char *message,
        core::Logger::Priority prio) noexcept
    {
        if (status == RPC_S_OK)
            return;

        auto ex = CreateException(status, message, "");
        core::Logger::Write(ex, prio);
    }

    /// <summary>
    /// Logs a RPC error.
    /// </summary>
    /// <param name="status">The status returned by the RPC API.</param>
    /// <param name="message">The main message for the error.</param>
    /// <param name="details">The details for the error.</param>
    /// <param name="prio">The priority for event to be logged.</param>
    void LogIfError(
        rpc_status_t status,
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
