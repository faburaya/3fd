#include "stdafx.h"
#include "preprocessing.h"
#include "rpc_impl_util.h"
#include "logger.h"

#include <string>
#include <sstream>
#include <codecvt>
#include <array>

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
    /// Flag to set stronger security options. Allowed cipher suites/algorithms will
    /// be SSL3 and TLS with MAC (weaker ones will be disabled to the detriment of
    /// interoperability) and revocation checks the whole certificate chain.
    /// </param>
    SChannelCredWrapper::SChannelCredWrapper(PCCERT_CONTEXT certCtxtHandle, bool strongerSec)
    try :
        m_credStructure{ 0 }
    {
        CALL_STACK_TRACE;

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
        CALL_STACK_TRACE;

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

    /////////////////////////
    // Error Helpers
    /////////////////////////

    /// <summary>
    /// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa379109.aspx.
    /// </summary>
    const std::array<const char *, 11> RpcErrorHelper::componentMap =
    {
        "UNKNOWN",
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
    /// Ensures the initialization of maps.
    /// </summary>
    void RpcErrorHelper::EnsureMapInitialization()
    {
        detectionLocationMap.emplace(10, "DealWithLRPCRequest10");
        detectionLocationMap.emplace(11, "DealWithLRPCRequest20");
        detectionLocationMap.emplace(12, "WithLRPCRequest30");
        detectionLocationMap.emplace(13, "WithLRPCRequest40");
        detectionLocationMap.emplace(20, "LrpcMessageToRpcMessage10");
        detectionLocationMap.emplace(21, "LrpcMessageToRpcMessage20");
        detectionLocationMap.emplace(22, "LrpcMessageToRpcMessage30");
        detectionLocationMap.emplace(30, "DealWithRequestMessage10");
        detectionLocationMap.emplace(31, "DealWithRequestMessage20");
        detectionLocationMap.emplace(32, "DealWithRequestMessage30");
        detectionLocationMap.emplace(40, "CheckSecurity10");
        detectionLocationMap.emplace(50, "DealWithBindMessage10");
        detectionLocationMap.emplace(51, "DealWithBindMessage20");
        detectionLocationMap.emplace(52, "DealWithBindMessage30");
        detectionLocationMap.emplace(53, "DealWithBindMessage40");
        detectionLocationMap.emplace(54, "DealWithBindMessage50");
        detectionLocationMap.emplace(55, "DealWithBindMessage60");
        detectionLocationMap.emplace(60, "FindServerCredentials10");
        detectionLocationMap.emplace(61, "FindServerCredentials20");
        detectionLocationMap.emplace(62, "FindServerCredentials30");
        detectionLocationMap.emplace(70, "AcceptFirstTime10");
        detectionLocationMap.emplace(71, "AcceptThirdLeg10");
        detectionLocationMap.emplace(72, "AcceptThirdLeg20");
        detectionLocationMap.emplace(73, "AcceptFirstTime20");
        detectionLocationMap.emplace(74, "AcceptThirdLeg40");
        detectionLocationMap.emplace(80, "AssociationRequested10");
        detectionLocationMap.emplace(81, "AssociationRequested20");
        detectionLocationMap.emplace(82, "AssociationRequested30");
        detectionLocationMap.emplace(90, "CompleteSecurityToken10");
        detectionLocationMap.emplace(91, "CompleteSecurityToken20");
        detectionLocationMap.emplace(100, "AcquireCredentialsForClient10");
        detectionLocationMap.emplace(101, "AcquireCredentialsForClient20");
        detectionLocationMap.emplace(102, "AcquireCredentialsForClient30");
        detectionLocationMap.emplace(110, "InquireDefaultPrincName10");
        detectionLocationMap.emplace(111, "InquireDefaultPrincName20");
        detectionLocationMap.emplace(120, "SignOrSeal10");
        detectionLocationMap.emplace(130, "VerifyOrUnseal10");
        detectionLocationMap.emplace(131, "VerifyOrUnseal20");
        detectionLocationMap.emplace(140, "InitializeFirstTime10");
        detectionLocationMap.emplace(141, "InitializeFirstTime20");
        detectionLocationMap.emplace(142, "InitializeFirstTime30");
        detectionLocationMap.emplace(150, "InitializeThirdLeg10");
        detectionLocationMap.emplace(151, "InitializeThirdLeg20");
        detectionLocationMap.emplace(152, "InitializeThirdLeg30");
        detectionLocationMap.emplace(153, "InitializeThirdLeg40");
        detectionLocationMap.emplace(154, "InitializeThirdLeg50");
        detectionLocationMap.emplace(155, "InitializeThirdLeg60");
        detectionLocationMap.emplace(160, "ImpersonateClient10");
        detectionLocationMap.emplace(170, "DispatchToStub10");
        detectionLocationMap.emplace(171, "DispatchToStub20");
        detectionLocationMap.emplace(180, "DispatchToStubWorker10");
        detectionLocationMap.emplace(181, "DispatchToStubWorker20");
        detectionLocationMap.emplace(182, "DispatchToStubWorker30");
        detectionLocationMap.emplace(183, "DispatchToStubWorker40");
        detectionLocationMap.emplace(190, "NMPOpen10");
        detectionLocationMap.emplace(191, "NMPOpen20");
        detectionLocationMap.emplace(192, "NMPOpen30");
        detectionLocationMap.emplace(193, "NMPOpen40");
        detectionLocationMap.emplace(200, "NMPSyncSend10");
        detectionLocationMap.emplace(210, "NMPSyncSendReceive10");
        detectionLocationMap.emplace(220, "NMPSyncSendReceive20");
        detectionLocationMap.emplace(221, "NMPSyncSendReceive30");
        detectionLocationMap.emplace(230, "COSend10");
        detectionLocationMap.emplace(240, "COSubmitRead10");
        detectionLocationMap.emplace(250, "COSubmitSyncRead10");
        detectionLocationMap.emplace(251, "COSubmitSyncRead20");
        detectionLocationMap.emplace(260, "COSyncRecv10");
        detectionLocationMap.emplace(270, "WSCheckForShutdowns10");
        detectionLocationMap.emplace(271, "WSCheckForShutdowns20");
        detectionLocationMap.emplace(272, "WSCheckForShutdowns30");
        detectionLocationMap.emplace(273, "WSCheckForShutdowns40");
        detectionLocationMap.emplace(274, "WSCheckForShutdowns50");
        detectionLocationMap.emplace(280, "WSSyncSend10");
        detectionLocationMap.emplace(281, "WSSyncSend20");
        detectionLocationMap.emplace(282, "WSSyncSend30");
        detectionLocationMap.emplace(290, "WSSyncRecv10");
        detectionLocationMap.emplace(291, "WSSyncRecv20");
        detectionLocationMap.emplace(292, "WSSyncRecv30");
        detectionLocationMap.emplace(300, "WSServerListenCommon10");
        detectionLocationMap.emplace(301, "WSServerListenCommon20");
        detectionLocationMap.emplace(302, "WSServerListenCommon30");
        detectionLocationMap.emplace(310, "WSOpen10");
        detectionLocationMap.emplace(311, "WSOpen20");
        detectionLocationMap.emplace(312, "WSOpen30");
        detectionLocationMap.emplace(313, "WSOpen40");
        detectionLocationMap.emplace(314, "WSOpen50");
        detectionLocationMap.emplace(315, "WSOpen60");
        detectionLocationMap.emplace(316, "WSOpen70");
        detectionLocationMap.emplace(317, "WSOpen80");
        detectionLocationMap.emplace(318, "WSOpen90");
        detectionLocationMap.emplace(320, "NetAddress10");
        detectionLocationMap.emplace(321, "NetAddress20");
        detectionLocationMap.emplace(322, "NetAddress30");
        detectionLocationMap.emplace(323, "NetAddress40");
        detectionLocationMap.emplace(330, "WSBind10");
        detectionLocationMap.emplace(331, "WSBind20");
        detectionLocationMap.emplace(332, "WSBind30");
        detectionLocationMap.emplace(333, "WSBind40");
        detectionLocationMap.emplace(334, "WSBind50");
        detectionLocationMap.emplace(335, "WSBind45");
        detectionLocationMap.emplace(340, "IPBuildAddressVector10");
        detectionLocationMap.emplace(350, "GetStatusForTimeout10");
        detectionLocationMap.emplace(351, "GetStatusForTimeout20");
        detectionLocationMap.emplace(360, "OSF_CCONNECTION__SendFragment10");
        detectionLocationMap.emplace(361, "OSF_CCONNECTION__SendFragment20");
        detectionLocationMap.emplace(370, "OSF_CCALL__ReceiveReply10");
        detectionLocationMap.emplace(371, "OSF_CCALL__ReceiveReply20");
        detectionLocationMap.emplace(380, "OSF_CCALL__FastSendReceive10");
        detectionLocationMap.emplace(381, "OSF_CCALL__FastSendReceive20");
        detectionLocationMap.emplace(382, "OSF_CCALL__FastSendReceive30");
        detectionLocationMap.emplace(390, "LRPC_BINDING_HANDLE__AllocateCCall10");
        detectionLocationMap.emplace(391, "LRPC_BINDING_HANDLE__AllocateCCall20");
        detectionLocationMap.emplace(400, "LRPC_ADDRESS__ServerSetupAddress10");
        detectionLocationMap.emplace(410, "LRPC_ADDRESS__HandleInvalidAssociationReference10");
        detectionLocationMap.emplace(420, "InitializeAuthzSupportIfNecessary10");
        detectionLocationMap.emplace(421, "InitializeAuthzSupportIfNecessary20");
        detectionLocationMap.emplace(430, "CreateDummyResourceManagerIfNecessary10");
        detectionLocationMap.emplace(431, "CreateDummyResourceManagerIfNecessary20");
        detectionLocationMap.emplace(440, "LRPC_SCALL__GetAuthorizationContet10");
        detectionLocationMap.emplace(441, "LRPC_SCALL__GetAuthorizationContet20");
        detectionLocationMap.emplace(442, "LRPC_SCALL__GetAuthorizationContet30");
        detectionLocationMap.emplace(450, "SCALL__DuplicateAuthzContet10");
        detectionLocationMap.emplace(460, "SCALL__CreateAndSaveAuthzContetFromToken10");
        detectionLocationMap.emplace(470, "SECURITY_CONTET__GetAccessToken10");
        detectionLocationMap.emplace(471, "SECURITY_CONTET__GetAccessToken20");
        detectionLocationMap.emplace(480, "OSF_SCALL__GetAuthorizationContet10");
        detectionLocationMap.emplace(500, "EpResolveEndpoint10");
        detectionLocationMap.emplace(501, "EpResolveEndpoint20");
        detectionLocationMap.emplace(510, "OSF_SCALL__GetBuffer10");
        detectionLocationMap.emplace(520, "LRPC_SCALL__ImpersonateClient10");
        detectionLocationMap.emplace(530, "SetMaimumLengths10");
        detectionLocationMap.emplace(540, "LRPC_CASSOCIATION__ActuallyDoBinding10");
        detectionLocationMap.emplace(541, "LRPC_CASSOCIATION__ActuallyDoBinding20");
        detectionLocationMap.emplace(542, "LRPC_CASSOCIATION__ActuallyDoBinding30");
        detectionLocationMap.emplace(543, "LRPC_CASSOCIATION__ActuallyDoBinding40");
        detectionLocationMap.emplace(550, "LRPC_CASSOCIATION__CreateBackConnection10");
        detectionLocationMap.emplace(551, "LRPC_CASSOCIATION__CreateBackConnection20");
        detectionLocationMap.emplace(552, "LRPC_CASSOCIATION__CreateBackConnection30");
        detectionLocationMap.emplace(560, "LRPC_CASSOCIATION__OpenLpcPort10");
        detectionLocationMap.emplace(561, "LRPC_CASSOCIATION__OpenLpcPort20");
        detectionLocationMap.emplace(562, "LRPC_CASSOCIATION__OpenLpcPort30");
        detectionLocationMap.emplace(563, "LRPC_CASSOCIATION__OpenLpcPort40");
        detectionLocationMap.emplace(570, "RegisterEntries10");
        detectionLocationMap.emplace(571, "RegisterEntries20");
        detectionLocationMap.emplace(580, "NDRSContetUnmarshall2_10");
        detectionLocationMap.emplace(581, "NDRSContetUnmarshall2_20");
        detectionLocationMap.emplace(582, "NDRSContetUnmarshall2_30");
        detectionLocationMap.emplace(583, "NDRSContetUnmarshall2_40");
        detectionLocationMap.emplace(584, "NDRSContetUnmarshall2_50");
        detectionLocationMap.emplace(590, "NDRSContetMarshall2_10");
        detectionLocationMap.emplace(600, "WinsockDatagramSend10");
        detectionLocationMap.emplace(601, "WinsockDatagramSend20");
        detectionLocationMap.emplace(610, "WinsockDatagramReceive10");
        detectionLocationMap.emplace(620, "WinsockDatagramSubmitReceive10");
        detectionLocationMap.emplace(630, "DG_CCALL__CancelAsyncCall10");
        detectionLocationMap.emplace(640, "DG_CCALL__DealWithTimeout10");
        detectionLocationMap.emplace(641, "DG_CCALL__DealWithTimeout20");
        detectionLocationMap.emplace(642, "DG_CCALL__DealWithTimeout30");
        detectionLocationMap.emplace(650, "DG_CCALL__DispatchPacket10");
        detectionLocationMap.emplace(660, "DG_CCALL__ReceiveSinglePacket10");
        detectionLocationMap.emplace(661, "DG_CCALL__ReceiveSinglePacket20");
        detectionLocationMap.emplace(662, "DG_CCALL__ReceiveSinglePacket30");
        detectionLocationMap.emplace(670, "WinsockDatagramResolve10");
        detectionLocationMap.emplace(680, "WinsockDatagramCreate10");
        detectionLocationMap.emplace(690, "TCP_QueryLocalAddress10");
        detectionLocationMap.emplace(691, "TCP_QueryLocalAddress20");
        detectionLocationMap.emplace(700, "OSF_CASSOCIATION__ProcessBindAckOrNak10");
        detectionLocationMap.emplace(701, "OSF_CASSOCIATION__ProcessBindAckOrNak20");
        detectionLocationMap.emplace(710, "MatchMsPrincipalName10");
        detectionLocationMap.emplace(720, "CompareRdnElement10");
        detectionLocationMap.emplace(730, "MatchFullPathPrincipalName10");
        detectionLocationMap.emplace(731, "MatchFullPathPrincipalName20");
        detectionLocationMap.emplace(732, "MatchFullPathPrincipalName30");
        detectionLocationMap.emplace(733, "MatchFullPathPrincipalName40");
        detectionLocationMap.emplace(734, "MatchFullPathPrincipalName50");
        detectionLocationMap.emplace(740, "RpcCertGeneratePrincipalName10");
        detectionLocationMap.emplace(741, "RpcCertGeneratePrincipalName20");
        detectionLocationMap.emplace(742, "RpcCertGeneratePrincipalName30");
        detectionLocationMap.emplace(750, "RpcCertVerifyContet10");
        detectionLocationMap.emplace(751, "RpcCertVerifyContet20");
        detectionLocationMap.emplace(752, "RpcCertVerifyContet30");
        detectionLocationMap.emplace(753, "RpcCertVerifyContet40");
        detectionLocationMap.emplace(761, "OSF_BINDING_HANDLE__NegotiateTransferSynta10");
    }

    /// <summary>
    /// Creates the exception.
    /// </summary>
    /// <param name="errCode">The error code.</param>
    /// <param name="message">The message.</param>
    /// <param name="details">The details.</param>
    /// <returns></returns>
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
            wchar_t errorMessage[DCE_C_ERROR_STRING_LEN];
            auto status = DceErrorInqTextW(errCode, errorMessage);

            if (status == RPC_S_OK)
            {
                // Remove the CRLF at the end of the error message:
                auto lastCharPos = wcslen(errorMessage) - 1;
                if (errorMessage[lastCharPos] == '\n' && errorMessage[lastCharPos - 1] == '\r')
                    errorMessage[lastCharPos] = errorMessage[lastCharPos - 1] = 0;
                
                oss << ": " << transcoder.to_bytes(errorMessage);
            }
            else
            {
                oss << ", but a secondary failure prevented the retrieval of details (";
                core::WWAPI::AppendDWordErrorMessage(status, nullptr, oss); 
                oss << ")";
            }

            // No details available?
            if (details.empty() || details == "")
                return core::AppException<std::runtime_error>(oss.str());

            // Try to enumerate extended error information:
            RPC_ERROR_ENUM_HANDLE errEnumHandle;
            status = RpcErrorStartEnumeration(&errEnumHandle);

            // No extended error info available?
            if(status == RPC_S_ENTRY_NOT_FOUND)
                return core::AppException<std::runtime_error>(oss.str(), details);
            
            // Failed to retrieve extended error info:
            if (status != RPC_S_OK)
            {
                oss << "\r\n\r\nSecondary failure prevented retrieval of extended error information";

                if (DceErrorInqTextW(status, errorMessage) == RPC_S_OK)
                {
                    // Remove the CRLF at the end of the error message:
                    auto lastCharPos = wcslen(errorMessage) - 1;
                    if (errorMessage[lastCharPos] == '\n' && errorMessage[lastCharPos - 1] == '\r')
                        errorMessage[lastCharPos] = errorMessage[lastCharPos - 1] = 0;

                    oss << ": " << transcoder.to_bytes(errorMessage);
                }
                else
                    oss << '!';

                oss << "\r\n";
            }

            oss << "\r\n\r\n=== Extended error information ===\r\n\r\n";

            struct KeyValuePair { int code; const char *name; };

            RPC_EXTENDED_ERROR_INFO errInfoEntry;
            errInfoEntry.Version = RPC_EEINFO_VERSION;
            errInfoEntry.Flags = 0;
            errInfoEntry.NumberOfParameters = 4;

            while ((status = RpcErrorGetNextRecord(&errEnumHandle, FALSE, &errInfoEntry)) == RPC_S_OK)
            {
                oss << "$ host " << (errInfoEntry.ComputerName != nullptr)
                                    ? transcoder.to_bytes(errInfoEntry.ComputerName)
                                    : "---";

                oss << " PID #" << errInfoEntry.ProcessID;

                oss << " @(" << errInfoEntry.u.SystemTime.wYear << '-'
                             << errInfoEntry.u.SystemTime.wMonth << '-'
                             << errInfoEntry.u.SystemTime.wDay << ' '
                             << errInfoEntry.u.SystemTime.wHour << ':'
                             << errInfoEntry.u.SystemTime.wMinute << ':'
                             << errInfoEntry.u.SystemTime.wSecond << ')';

                oss << " [com " << errInfoEntry.GeneratingComponent
                    << ", loc " << errInfoEntry.DetectionLocation
                    << ", sta " << errInfoEntry.Status << ']';

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
                        oss << std::hex << value.PVal;
                        break;
                    case eeptBinary:
                        break; // skip (for RPC runtime use only)
                    case eeptNone:
                        break; // skip (truncated string)
                    default:
                        oss << "???";
                        break;
                    }
                }

                oss << " }\n";

                errInfoEntry.Version = RPC_EEINFO_VERSION;
                errInfoEntry.Flags = 0;
                errInfoEntry.NumberOfParameters = 4;
            }
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

        throw CreateException(status, message, "");
    }

    /// <summary>
    /// Throws an exception for a RPC error.
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
    /// Logs a RPC error.
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
    /// Logs a RPC error.
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
