#include "pch.h"
#include "rpc_impl_util.h"
#include <3fd/core/logger.h>

#include <algorithm>
#include <array>
#include <codecvt>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

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
    /// <param name="authnLevel">The authentication level option to convert.</param>
    /// <returns>A string with a label for the authentication level.</returns>
    const char *ToString(AuthenticationLevel authnLevel)
    {
        switch (authnLevel)
        {
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
        case RPC_C_AUTHN_WINNT:
            return R"(authentication service "Microsoft NTLM SSP")";

        case RPC_C_AUTHN_GSS_NEGOTIATE:
            return R"(authentication service "Microsoft Negotiate SSP")";

        case RPC_C_AUTHN_GSS_KERBEROS:
            return R"(authentication service "Microsoft Kerberos SSP")";

        case RPC_C_AUTHN_GSS_SCHANNEL:
            return R"(authentication service "Schannel SSP")";

        default:
            _ASSERTE(false);
            return "UNRECOGNIZED AUTHENTICATION SERVICE";
        }
    }


    /// <summary>
    /// Gets a structure with security QOS options for Microsoft RPC,
    /// generates a text description for it and append to output stream.
    /// </summary>
    /// <param name="secQOS">An structure used by Microsoft RPC implementation to hold security QOS options.</param>
    /// <param name="oss">The output string stream.</param>
    void AppendSecQosOptsDescription(const RPC_SECURITY_QOS &secQOS, std::ostringstream &oss)
    {
        if ((secQOS.Capabilities & RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH) != 0)
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
            m_ptrs2Uuids.push_back(dbg_new UUID(uuid));
        else
        {
            std::ostringstream oss;
            oss << "Could not copy object UUID because the amount of "
                   "implementations for the RPC interface exceeded "
                   "a practical limit of " << UUID_VECTOR_MAX_SIZE;

            throw core::AppException<std::length_error>(oss.str());
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
    /// The certificate store location (such as CERT_SYSTEM_STORE_CURRENT_USER or
    /// CERT_SYSTEM_STORE_LOCAL_MACHINE) that contains the specified certificate.
    /// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa388136.aspx.
    /// </param>
    /// <param name="storeName">The certificate store name
    /// (such as "My") that contains the specified certificate.</param>
    SystemCertificateStore::SystemCertificateStore(DWORD registryLocation, const string &storeName)
    try :
        m_certStoreHandle(nullptr)
    {
        CALL_STACK_TRACE;

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
        throw; // just forward an exception regarding an error known to have been already handled
    }
    catch (std::exception &ex)
    {
        std::ostringstream oss;
        oss << "Generic failure when opening system certificate store: " << ex.what();
        throw core::AppException<std::runtime_error>(oss.str());
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
            throw; // just forward an exception regarding an error known to have been already handled
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
    /// Flag to set stronger security options. It disables known weak cryptographic algorithms,
    /// cipher suites, and SSL/TLS protocol versions that may be otherwise enabled for better
    /// interoperability. Plus, revocation checks the whole certificate chain.
    /// </param>
    SChannelCredWrapper::SChannelCredWrapper(PCCERT_CONTEXT certCtxtHandle, bool strongerSec)
    try :
        m_credStructure{ 0 }
    {
        CALL_STACK_TRACE;

        m_credStructure.dwVersion = SCHANNEL_CRED_VERSION;
        m_credStructure.cCreds = 1;
        m_credStructure.paCred = dbg_new PCCERT_CONTEXT[1]{ certCtxtHandle };

        if (strongerSec)
        {
            m_credStructure.dwFlags = SCH_CRED_REVOCATION_CHECK_CHAIN;
#   ifndef _USING_V110_SDK71_
            m_credStructure.dwFlags |= SCH_USE_STRONG_CRYPTO;
#   endif
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
    /// Flag to set stronger security options. It disables known weak cryptographic algorithms,
    /// cipher suites, and SSL/TLS protocol versions that may be otherwise enabled for better
    /// interoperability. Plus, revocation checks the whole certificate chain.
    ///</param>
    SChannelCredWrapper::SChannelCredWrapper(HCERTSTORE certStoreHandle,
                                             PCCERT_CONTEXT certCtxtHandle,
                                             bool strongerSec)
    try :
        m_credStructure{ 0 }
    {
        CALL_STACK_TRACE;

        m_credStructure.dwVersion = SCHANNEL_CRED_VERSION;
        m_credStructure.cCreds = 1;
        m_credStructure.paCred = dbg_new PCCERT_CONTEXT[1]{ certCtxtHandle };
        m_credStructure.hRootStore = certStoreHandle;

        if (strongerSec)
        {
            m_credStructure.dwFlags = SCH_CRED_REVOCATION_CHECK_CHAIN;
#   ifndef _USING_V110_SDK71_
            m_credStructure.dwFlags |= SCH_USE_STRONG_CRYPTO;
#   endif
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


    /////////////////////////
    // Error Helpers
    /////////////////////////

    /// <summary>
    /// Maps codes for components to labels.
    /// See http://msdn.microsoft.com/en-us/library/windows/desktop/aa379109.aspx.
    /// </summary>
    const std::array<const char *, 11> RpcErrorHelper::componentMap =
    {
        "???", // unknown or code out of range
        "Application",
        "Runtime",
        "Security Provider",
        "NPFS",
        "RDR",
        "NMP",
        "IO",
        "Winsock",
        "Authz code",
        "LPC"
    };

    /// <summary>
    /// Maps codes for detection locations to labels.
    /// See http://msdn.microsoft.com/en-us/library/windows/desktop/aa373838.aspx.
    /// </summary>
    const RpcCodeLabelKVPair RpcErrorHelper::detectionLocationMap[] =
    {
        RpcCodeLabelKVPair(10, "DealWithLRPCRequest10"),
        RpcCodeLabelKVPair(11, "DealWithLRPCRequest20"),
        RpcCodeLabelKVPair(12, "WithLRPCRequest30"),
        RpcCodeLabelKVPair(13, "WithLRPCRequest40"),
        RpcCodeLabelKVPair(20, "LrpcMessageToRpcMessage10"),
        RpcCodeLabelKVPair(21, "LrpcMessageToRpcMessage20"),
        RpcCodeLabelKVPair(22, "LrpcMessageToRpcMessage30"),
        RpcCodeLabelKVPair(30, "DealWithRequestMessage10"),
        RpcCodeLabelKVPair(31, "DealWithRequestMessage20"),
        RpcCodeLabelKVPair(32, "DealWithRequestMessage30"),
        RpcCodeLabelKVPair(40, "CheckSecurity10"),
        RpcCodeLabelKVPair(50, "DealWithBindMessage10"),
        RpcCodeLabelKVPair(51, "DealWithBindMessage20"),
        RpcCodeLabelKVPair(52, "DealWithBindMessage30"),
        RpcCodeLabelKVPair(53, "DealWithBindMessage40"),
        RpcCodeLabelKVPair(54, "DealWithBindMessage50"),
        RpcCodeLabelKVPair(55, "DealWithBindMessage60"),
        RpcCodeLabelKVPair(60, "FindServerCredentials10"),
        RpcCodeLabelKVPair(61, "FindServerCredentials20"),
        RpcCodeLabelKVPair(62, "FindServerCredentials30"),
        RpcCodeLabelKVPair(70, "AcceptFirstTime10"),
        RpcCodeLabelKVPair(71, "AcceptThirdLeg10"),
        RpcCodeLabelKVPair(72, "AcceptThirdLeg20"),
        RpcCodeLabelKVPair(73, "AcceptFirstTime20"),
        RpcCodeLabelKVPair(74, "AcceptThirdLeg40"),
        RpcCodeLabelKVPair(80, "AssociationRequested10"),
        RpcCodeLabelKVPair(81, "AssociationRequested20"),
        RpcCodeLabelKVPair(82, "AssociationRequested30"),
        RpcCodeLabelKVPair(90, "CompleteSecurityToken10"),
        RpcCodeLabelKVPair(91, "CompleteSecurityToken20"),
        RpcCodeLabelKVPair(100, "AcquireCredentialsForClient10"),
        RpcCodeLabelKVPair(101, "AcquireCredentialsForClient20"),
        RpcCodeLabelKVPair(102, "AcquireCredentialsForClient30"),
        RpcCodeLabelKVPair(110, "InquireDefaultPrincName10"),
        RpcCodeLabelKVPair(111, "InquireDefaultPrincName20"),
        RpcCodeLabelKVPair(120, "SignOrSeal10"),
        RpcCodeLabelKVPair(130, "VerifyOrUnseal10"),
        RpcCodeLabelKVPair(131, "VerifyOrUnseal20"),
        RpcCodeLabelKVPair(140, "InitializeFirstTime10"),
        RpcCodeLabelKVPair(141, "InitializeFirstTime20"),
        RpcCodeLabelKVPair(142, "InitializeFirstTime30"),
        RpcCodeLabelKVPair(150, "InitializeThirdLeg10"),
        RpcCodeLabelKVPair(151, "InitializeThirdLeg20"),
        RpcCodeLabelKVPair(152, "InitializeThirdLeg30"),
        RpcCodeLabelKVPair(153, "InitializeThirdLeg40"),
        RpcCodeLabelKVPair(154, "InitializeThirdLeg50"),
        RpcCodeLabelKVPair(155, "InitializeThirdLeg60"),
        RpcCodeLabelKVPair(160, "ImpersonateClient10"),
        RpcCodeLabelKVPair(170, "DispatchToStub10"),
        RpcCodeLabelKVPair(171, "DispatchToStub20"),
        RpcCodeLabelKVPair(180, "DispatchToStubWorker10"),
        RpcCodeLabelKVPair(181, "DispatchToStubWorker20"),
        RpcCodeLabelKVPair(182, "DispatchToStubWorker30"),
        RpcCodeLabelKVPair(183, "DispatchToStubWorker40"),
        RpcCodeLabelKVPair(190, "NMPOpen10"),
        RpcCodeLabelKVPair(191, "NMPOpen20"),
        RpcCodeLabelKVPair(192, "NMPOpen30"),
        RpcCodeLabelKVPair(193, "NMPOpen40"),
        RpcCodeLabelKVPair(200, "NMPSyncSend10"),
        RpcCodeLabelKVPair(210, "NMPSyncSendReceive10"),
        RpcCodeLabelKVPair(220, "NMPSyncSendReceive20"),
        RpcCodeLabelKVPair(221, "NMPSyncSendReceive30"),
        RpcCodeLabelKVPair(230, "COSend10"),
        RpcCodeLabelKVPair(240, "COSubmitRead10"),
        RpcCodeLabelKVPair(250, "COSubmitSyncRead10"),
        RpcCodeLabelKVPair(251, "COSubmitSyncRead20"),
        RpcCodeLabelKVPair(260, "COSyncRecv10"),
        RpcCodeLabelKVPair(270, "WSCheckForShutdowns10"),
        RpcCodeLabelKVPair(271, "WSCheckForShutdowns20"),
        RpcCodeLabelKVPair(272, "WSCheckForShutdowns30"),
        RpcCodeLabelKVPair(273, "WSCheckForShutdowns40"),
        RpcCodeLabelKVPair(274, "WSCheckForShutdowns50"),
        RpcCodeLabelKVPair(280, "WSSyncSend10"),
        RpcCodeLabelKVPair(281, "WSSyncSend20"),
        RpcCodeLabelKVPair(282, "WSSyncSend30"),
        RpcCodeLabelKVPair(290, "WSSyncRecv10"),
        RpcCodeLabelKVPair(291, "WSSyncRecv20"),
        RpcCodeLabelKVPair(292, "WSSyncRecv30"),
        RpcCodeLabelKVPair(300, "WSServerListenCommon10"),
        RpcCodeLabelKVPair(301, "WSServerListenCommon20"),
        RpcCodeLabelKVPair(302, "WSServerListenCommon30"),
        RpcCodeLabelKVPair(310, "WSOpen10"),
        RpcCodeLabelKVPair(311, "WSOpen20"),
        RpcCodeLabelKVPair(312, "WSOpen30"),
        RpcCodeLabelKVPair(313, "WSOpen40"),
        RpcCodeLabelKVPair(314, "WSOpen50"),
        RpcCodeLabelKVPair(315, "WSOpen60"),
        RpcCodeLabelKVPair(316, "WSOpen70"),
        RpcCodeLabelKVPair(317, "WSOpen80"),
        RpcCodeLabelKVPair(318, "WSOpen90"),
        RpcCodeLabelKVPair(320, "NetAddress10"),
        RpcCodeLabelKVPair(321, "NetAddress20"),
        RpcCodeLabelKVPair(322, "NetAddress30"),
        RpcCodeLabelKVPair(323, "NetAddress40"),
        RpcCodeLabelKVPair(330, "WSBind10"),
        RpcCodeLabelKVPair(331, "WSBind20"),
        RpcCodeLabelKVPair(332, "WSBind30"),
        RpcCodeLabelKVPair(333, "WSBind40"),
        RpcCodeLabelKVPair(334, "WSBind50"),
        RpcCodeLabelKVPair(335, "WSBind45"),
        RpcCodeLabelKVPair(340, "IPBuildAddressVector10"),
        RpcCodeLabelKVPair(350, "GetStatusForTimeout10"),
        RpcCodeLabelKVPair(351, "GetStatusForTimeout20"),
        RpcCodeLabelKVPair(360, "OSF_CCONNECTION__SendFragment10"),
        RpcCodeLabelKVPair(361, "OSF_CCONNECTION__SendFragment20"),
        RpcCodeLabelKVPair(370, "OSF_CCALL__ReceiveReply10"),
        RpcCodeLabelKVPair(371, "OSF_CCALL__ReceiveReply20"),
        RpcCodeLabelKVPair(380, "OSF_CCALL__FastSendReceive10"),
        RpcCodeLabelKVPair(381, "OSF_CCALL__FastSendReceive20"),
        RpcCodeLabelKVPair(382, "OSF_CCALL__FastSendReceive30"),
        RpcCodeLabelKVPair(390, "LRPC_BINDING_HANDLE__AllocateCCall10"),
        RpcCodeLabelKVPair(391, "LRPC_BINDING_HANDLE__AllocateCCall20"),
        RpcCodeLabelKVPair(400, "LRPC_ADDRESS__ServerSetupAddress10"),
        RpcCodeLabelKVPair(410, "LRPC_ADDRESS__HandleInvalidAssociationReference10"),
        RpcCodeLabelKVPair(420, "InitializeAuthzSupportIfNecessary10"),
        RpcCodeLabelKVPair(421, "InitializeAuthzSupportIfNecessary20"),
        RpcCodeLabelKVPair(430, "CreateDummyResourceManagerIfNecessary10"),
        RpcCodeLabelKVPair(431, "CreateDummyResourceManagerIfNecessary20"),
        RpcCodeLabelKVPair(440, "LRPC_SCALL__GetAuthorizationContet10"),
        RpcCodeLabelKVPair(441, "LRPC_SCALL__GetAuthorizationContet20"),
        RpcCodeLabelKVPair(442, "LRPC_SCALL__GetAuthorizationContet30"),
        RpcCodeLabelKVPair(450, "SCALL__DuplicateAuthzContet10"),
        RpcCodeLabelKVPair(460, "SCALL__CreateAndSaveAuthzContetFromToken10"),
        RpcCodeLabelKVPair(470, "SECURITY_CONTET__GetAccessToken10"),
        RpcCodeLabelKVPair(471, "SECURITY_CONTET__GetAccessToken20"),
        RpcCodeLabelKVPair(480, "OSF_SCALL__GetAuthorizationContet10"),
        RpcCodeLabelKVPair(500, "EpResolveEndpoint10"),
        RpcCodeLabelKVPair(501, "EpResolveEndpoint20"),
        RpcCodeLabelKVPair(510, "OSF_SCALL__GetBuffer10"),
        RpcCodeLabelKVPair(520, "LRPC_SCALL__ImpersonateClient10"),
        RpcCodeLabelKVPair(530, "SetMaimumLengths10"),
        RpcCodeLabelKVPair(540, "LRPC_CASSOCIATION__ActuallyDoBinding10"),
        RpcCodeLabelKVPair(541, "LRPC_CASSOCIATION__ActuallyDoBinding20"),
        RpcCodeLabelKVPair(542, "LRPC_CASSOCIATION__ActuallyDoBinding30"),
        RpcCodeLabelKVPair(543, "LRPC_CASSOCIATION__ActuallyDoBinding40"),
        RpcCodeLabelKVPair(550, "LRPC_CASSOCIATION__CreateBackConnection10"),
        RpcCodeLabelKVPair(551, "LRPC_CASSOCIATION__CreateBackConnection20"),
        RpcCodeLabelKVPair(552, "LRPC_CASSOCIATION__CreateBackConnection30"),
        RpcCodeLabelKVPair(560, "LRPC_CASSOCIATION__OpenLpcPort10"),
        RpcCodeLabelKVPair(561, "LRPC_CASSOCIATION__OpenLpcPort20"),
        RpcCodeLabelKVPair(562, "LRPC_CASSOCIATION__OpenLpcPort30"),
        RpcCodeLabelKVPair(563, "LRPC_CASSOCIATION__OpenLpcPort40"),
        RpcCodeLabelKVPair(570, "RegisterEntries10"),
        RpcCodeLabelKVPair(571, "RegisterEntries20"),
        RpcCodeLabelKVPair(580, "NDRSContetUnmarshall2_10"),
        RpcCodeLabelKVPair(581, "NDRSContetUnmarshall2_20"),
        RpcCodeLabelKVPair(582, "NDRSContetUnmarshall2_30"),
        RpcCodeLabelKVPair(583, "NDRSContetUnmarshall2_40"),
        RpcCodeLabelKVPair(584, "NDRSContetUnmarshall2_50"),
        RpcCodeLabelKVPair(590, "NDRSContetMarshall2_10"),
        RpcCodeLabelKVPair(600, "WinsockDatagramSend10"),
        RpcCodeLabelKVPair(601, "WinsockDatagramSend20"),
        RpcCodeLabelKVPair(610, "WinsockDatagramReceive10"),
        RpcCodeLabelKVPair(620, "WinsockDatagramSubmitReceive10"),
        RpcCodeLabelKVPair(630, "DG_CCALL__CancelAsyncCall10"),
        RpcCodeLabelKVPair(640, "DG_CCALL__DealWithTimeout10"),
        RpcCodeLabelKVPair(641, "DG_CCALL__DealWithTimeout20"),
        RpcCodeLabelKVPair(642, "DG_CCALL__DealWithTimeout30"),
        RpcCodeLabelKVPair(650, "DG_CCALL__DispatchPacket10"),
        RpcCodeLabelKVPair(660, "DG_CCALL__ReceiveSinglePacket10"),
        RpcCodeLabelKVPair(661, "DG_CCALL__ReceiveSinglePacket20"),
        RpcCodeLabelKVPair(662, "DG_CCALL__ReceiveSinglePacket30"),
        RpcCodeLabelKVPair(670, "WinsockDatagramResolve10"),
        RpcCodeLabelKVPair(680, "WinsockDatagramCreate10"),
        RpcCodeLabelKVPair(690, "TCP_QueryLocalAddress10"),
        RpcCodeLabelKVPair(691, "TCP_QueryLocalAddress20"),
        RpcCodeLabelKVPair(700, "OSF_CASSOCIATION__ProcessBindAckOrNak10"),
        RpcCodeLabelKVPair(701, "OSF_CASSOCIATION__ProcessBindAckOrNak20"),
        RpcCodeLabelKVPair(710, "MatchMsPrincipalName10"),
        RpcCodeLabelKVPair(720, "CompareRdnElement10"),
        RpcCodeLabelKVPair(730, "MatchFullPathPrincipalName10"),
        RpcCodeLabelKVPair(731, "MatchFullPathPrincipalName20"),
        RpcCodeLabelKVPair(732, "MatchFullPathPrincipalName30"),
        RpcCodeLabelKVPair(733, "MatchFullPathPrincipalName40"),
        RpcCodeLabelKVPair(734, "MatchFullPathPrincipalName50"),
        RpcCodeLabelKVPair(740, "RpcCertGeneratePrincipalName10"),
        RpcCodeLabelKVPair(741, "RpcCertGeneratePrincipalName20"),
        RpcCodeLabelKVPair(742, "RpcCertGeneratePrincipalName30"),
        RpcCodeLabelKVPair(750, "RpcCertVerifyContet10"),
        RpcCodeLabelKVPair(751, "RpcCertVerifyContet20"),
        RpcCodeLabelKVPair(752, "RpcCertVerifyContet30"),
        RpcCodeLabelKVPair(753, "RpcCertVerifyContet40"),
        RpcCodeLabelKVPair(761, "OSF_BINDING_HANDLE__NegotiateTransferSynta10")
    };


    /// <summary>
    /// Gets the label for a component, given its code
    /// coming from extended RPC error information.
    /// </summary>
    /// <param name="code">The code for the component.</param>
    /// <returns>The corresponding label for the given component code.</returns>
    const char * RpcErrorHelper::GetComponentLabel(uint32_t code)
    {
        // Out of range?
        if (code < 1 || code >= componentMap.size())
            return componentMap[0];

        return componentMap[code];
    }


    /// <summary>
    /// Gets the label for a detection location, given its code
    /// comming from extended RPC error information.
    /// </summary>
    /// <param name="code">The code for the detection location.</param>
    /// <returns>The corresponding label for the given detection location ID.</returns>
    const char * RpcErrorHelper::GetDetectionLocationLabel(uint32_t code)
    {
        auto iter = std::lower_bound(
            std::begin(detectionLocationMap),
            std::end(detectionLocationMap),
            RpcCodeLabelKVPair(code, ""),
            [](const RpcCodeLabelKVPair &left, const RpcCodeLabelKVPair &right)
            {
                return (left.first < right.first);
            }
        );

        if (std::end(detectionLocationMap) != iter && iter->first == code)
            return iter->second;
        else
            return "???"; // code unknown or out-of-range
    }


    /* Wraps 'DceErrorInqTextW' in order to make it return pretty text
       for the provided error code, but does not recursively generates
       messages for possible failures during this process. */
    static string GetFirstLevelRpcErrorText(
        RPC_STATUS errCode,
        std::wstring_convert<std::codecvt_utf8<wchar_t>> &transcoder,
        RPC_STATUS &status)
    {
        wchar_t errorMessage[DCE_C_ERROR_STRING_LEN];
        status = DceErrorInqTextW(errCode, errorMessage);

        if (status != RPC_S_OK)
            return "";

        // Remove the CRLF at the end of the error message:
        auto lastCharPos = wcslen(errorMessage) - 1;
        if (errorMessage[lastCharPos] == '\n' && errorMessage[lastCharPos - 1] == '\r')
            errorMessage[lastCharPos] = errorMessage[lastCharPos - 1] = 0;

        return transcoder.to_bytes(errorMessage);
    }


    /// <summary>
    /// Creates an exception given the information from parameters.
    /// </summary>
    /// <param name="errCode">The error code (from RPC runtime).</param>
    /// <param name="message">The message.</param>
    /// <param name="details">The details.</param>
    /// <returns>The assembled exception object.</returns>
    core::AppException<std::runtime_error>
    RpcErrorHelper::CreateException(RPC_STATUS errCode,
                                    const string &message,
                                    const string &details)
    {
        try
        {
            // Assemble the message:
            std::ostringstream oss;
            oss << message << " - RPC runtime reported an error";

            // Get error message from API:
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            
            RPC_STATUS status;
            auto message = GetFirstLevelRpcErrorText(errCode, transcoder, status);

            if (status == RPC_S_OK)
                oss << ": " << message;
            else
            {
                oss << ", but a secondary failure prevented the retrieval of details (";
                core::WWAPI::AppendDWordErrorMessage(status, nullptr, oss); 
                oss << ")";
            }

            // No details available?
            if (details.empty() || details == "")
                return core::AppException<std::runtime_error>(oss.str());

#   ifndef ENABLE_3FD_ERR_IMPL_DETAILS
            return core::AppException<std::runtime_error>(oss.str(), details);
#   else
            // Try to enumerate extended error information:
            RPC_ERROR_ENUM_HANDLE errEnumHandle;
            status = RpcErrorStartEnumeration(&errEnumHandle);

            // No extended error info available?
            if(status == RPC_S_ENTRY_NOT_FOUND)
                return core::AppException<std::runtime_error>(oss.str(), details);
            
            // Extended error info will be placed after details:
            auto what = oss.str();
            oss.str("");
            oss << details;

            // Failed to retrieve extended error info:
            if (status != RPC_S_OK)
            {
                oss << "\r\n\r\nSecondary failure prevented retrieval of extended error information";

                RPC_STATUS status2;
                message = GetFirstLevelRpcErrorText(status, transcoder, status2);

                if (status2 == RPC_S_OK)
                    oss << ": " << message;
                else
                    oss << '!';

                return core::AppException<std::runtime_error>(what, oss.str());
            }

            oss << "\r\n\r\n=== Extended error information ===\r\n";

            RPC_EXTENDED_ERROR_INFO errInfoEntry;
            errInfoEntry.Version = RPC_EEINFO_VERSION;
            errInfoEntry.Flags = 0;
            errInfoEntry.NumberOfParameters = 4;

            while ((status = RpcErrorGetNextRecord(&errEnumHandle, FALSE, &errInfoEntry)) == RPC_S_OK)
            {
                oss << "\r\n";

                if ((errInfoEntry.Flags & EEInfoPreviousRecordsMissing) != 0)
                    oss << "$ *** missing record(s) ***";

                oss << "$ host " << (
                    errInfoEntry.ComputerName != nullptr
                        ? transcoder.to_bytes(errInfoEntry.ComputerName)
                        : "---"
                );

                oss << " / PID #" << errInfoEntry.ProcessID;

                oss << " @(" << std::setw(2) << std::setfill('0')
                    << errInfoEntry.u.SystemTime.wYear << '-'
                    << errInfoEntry.u.SystemTime.wMonth << '-'
                    << errInfoEntry.u.SystemTime.wDay << ' '
                    << errInfoEntry.u.SystemTime.wHour << ':'
                    << errInfoEntry.u.SystemTime.wMinute << ':'
                    << errInfoEntry.u.SystemTime.wSecond << ')';

                oss << " [com:" << GetComponentLabel(errInfoEntry.GeneratingComponent)
                    << "; loc:" << GetDetectionLocationLabel(errInfoEntry.DetectionLocation)
                    << "; status=" << errInfoEntry.Status << ']';

                oss << " { ";

                for (int idx = 0; idx < errInfoEntry.NumberOfParameters; ++idx)
                {
                    if (idx != 0)
                        oss << ", ";

                    auto &value = errInfoEntry.Parameters[idx].u;

                    switch (errInfoEntry.Parameters[idx].ParameterType)
                    {
                    case eeptAnsiString:
                        oss << '\"' << value.AnsiString << '\"';
                        break;
                    case eeptUnicodeString:
                        oss << '\"' << transcoder.to_bytes(value.UnicodeString) << '\"';
                        break;
                    case eeptLongVal:
                        oss << value.LVal;
                        break;
                    case eeptShortVal:
                        oss << value.SVal;
                        break;
                    case eeptPointerVal:
                        oss << std::hex << value.PVal << std::dec;
                        break;
                    case eeptBinary:
                        break; // skip (for RPC runtime use only)
                    case eeptNone:
                        break; // skip (truncated string)
                    default:
                        oss << "???";
                        break;
                    }
                }// for loop end

                oss << " }";

                if ((errInfoEntry.Flags & EEInfoNextRecordsMissing) != 0)
                    oss << "\r\n$ *** missing record(s) ***";

                errInfoEntry.Version = RPC_EEINFO_VERSION;
                errInfoEntry.Flags = 0;
                errInfoEntry.NumberOfParameters = 4;
            }// while loop end

            // End of records: release resources
            RpcErrorEndEnumeration(&errEnumHandle);

            // Loop ended due to failure when processing a record?
            if (status != RPC_S_ENTRY_NOT_FOUND)
            {
                RPC_STATUS status2;
                message = GetFirstLevelRpcErrorText(status, transcoder, status2);
                oss << "\r\n$ Failed to retrieve record! " << message;
            }

            return core::AppException<std::runtime_error>(what, oss.str());
#   endif
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << message << " - RPC runtime reported an error, but a generic "
                   "failure prevented the retrieval of more information: " << ex.what();

            return core::AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Throws an exception for a RPC runtime error.
    /// </summary>
    /// <param name="status">The status returned by the RPC API.</param>
    /// <param name="message">The main message for the error.</param>
    void ThrowIfError(RPC_STATUS status, const char *message)
    {
        if (status == RPC_S_OK)
            return;

        throw RpcErrorHelper::CreateException(status, message, "");
    }


    /// <summary>
    /// Throws an exception for a RPC error.
    /// </summary>
    /// <param name="status">The status returned by the RPC API.</param>
    /// <param name="message">The main message for the error.</param>
    /// <param name="details">The details for the error.</param>
    void ThrowIfError(RPC_STATUS status,
                      const char *message,
                      const string &details)
    {
        if (status == RPC_S_OK)
            return;
            
        throw RpcErrorHelper::CreateException(status, message, details);
    }


    /// <summary>
    /// Logs a RPC error.
    /// </summary>
    /// <param name="status">The status returned by the RPC API.</param>
    /// <param name="message">The main message for the error.</param>
    /// <param name="prio">The priority for event to be logged.</param>
    void LogIfError(RPC_STATUS status,
                    const char *message,
                    core::Logger::Priority prio) noexcept
    {
        if (status == RPC_S_OK)
            return;

        auto ex = RpcErrorHelper::CreateException(status, message, "");
        core::Logger::Write(ex, prio);
    }


    /// <summary>
    /// Logs a RPC error.
    /// </summary>
    /// <param name="status">The status returned by the RPC API.</param>
    /// <param name="message">The main message for the error.</param>
    /// <param name="details">The details for the error.</param>
    /// <param name="prio">The priority for event to be logged.</param>
    void LogIfError(RPC_STATUS status,
                    const char *message,
                    const string &details,
                    core::Logger::Priority prio) noexcept
    {
        if (status == RPC_S_OK)
            return;

        auto ex = RpcErrorHelper::CreateException(status, message, details);
        core::Logger::Write(ex, prio);
    }

}// end of namespace rpc
}// end of namespace _3fd
