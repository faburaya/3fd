#include "stdafx.h"
#include "runtime.h"
#include "callstacktracer.h"
#include "rpc_helpers.h"
#include "rpc_test_shared.h"

#ifdef _WIN64
#   include "AcmeTesting_x64.h"
#else
#   include "AcmeTesting_w32.h"
#endif

#include <thread>
#include <wincrypt.h>

////////////////////////////
// RPC Memory Allocation
////////////////////////////

void * __RPC_USER MIDL_user_allocate(size_t qtBytes)
{
    return (void __RPC_FAR *)malloc(qtBytes);
}

void __RPC_USER MIDL_user_free(void *ptr)
{
    free(ptr);
}

namespace _3fd
{
namespace integration_tests
{
    using namespace core;
    using namespace rpc;

    void HandleException();

    RPC_STATUS OperateImpl(RPC_BINDING_HANDLE handle, double left, double right, double &result)
    {
    RpcTryExcept
        Operate(handle, left, right, &result);
        return RPC_S_OK;
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
        return RpcExceptionCode();
    RpcEndExcept
    }

    RPC_STATUS ChangeCaseImpl(RPC_BINDING_HANDLE handle, const char *text, cstring &output)
    {
    RpcTryExcept
        cstring input = { strlen(text) + 1, (unsigned char *)text };
        ChangeCase(handle, &input, &output);
        return RPC_S_OK;
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
        return RpcExceptionCode();
    RpcEndExcept
    }

    /// <summary>
    /// Proxy for AcmeTesting RPC server.
    /// </summary>
    class AcmeRpcClient : public rpc::RpcClient
    {
    public:

        using RpcClient::RpcClient;

        double Operate(double left, double right)
        {
            double result;
            auto status = OperateImpl(GetBindingHandle(), left, right, result);
            ThrowIfError(status, "Failed to invoke RPC client stub routine \'Operate\'");
            return result;
        }

        string ChangeCase(const char *text)
        {
            /* When the stubs have been generated for OSF compliance
            the output string parameter must fulfill the memory allocation
            of the buffer carrying the text:

            unsigned char buffer[128];
            cstring output { sizeof buffer, buffer }; */
            cstring output;
            auto status = ChangeCaseImpl(GetBindingHandle(), text, output);
            ThrowIfError(status, "Failed to invoke RPC client stub routine \'ChangeCase\'");
            return string(output.data, output.data + output.size - 1);
        }

        void WriteOnStorage()
        {
            ::WriteOnStorage(GetBindingHandle());
        }

        uint32_t Shutdown()
        {
            return ::Shutdown(GetBindingHandle());
        }
    };

    /// <summary>
    /// Tests RPC client issuing requests without authentication.
    /// </summary>
    TEST(Framework_RpcNoAuth_TestCase1, ClientRun_RequestTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            // Wait for RPC server to become available
            system("pause");

            AcmeRpcClient client1(
                ProtocolSequence::Local,
                objectsUuidsImpl1[5],
                "TARS"//"MyVirtualServer.MyDomain.local"
            );

            EXPECT_EQ(696.0, client1.Operate(6.0, 116.0));
            EXPECT_EQ("SQUIRREL", client1.ChangeCase("squirrel"));

            AcmeRpcClient client2(
                ProtocolSequence::Local,
                objectsUuidsImpl2[5],
                "TARS"//"MyVirtualServer.MyDomain.local"
            );

            EXPECT_EQ(696.0, client2.Operate(606.0, 90.0));
            EXPECT_EQ("squirrel", client2.ChangeCase("SQUIRREL"));

            client2.Shutdown();
        }
        catch (...)
        {
            HandleException();
        }
    }

    // The set of options for each test template instantiation
    struct AuthnTestOptions
    {
        ProtocolSequence protocolSequence;
        const char *objectUUID1;
        const char *objectUUID2;
        AuthenticationLevel authenticationLevel;
        AuthenticationSecurity authenticationSecurity;
        ImpersonationLevel impersonationLevel;
    };

    /// <summary>
    /// Test case for RPC client with NTLM/Kerberos/Negotiate authentication.
    /// </summary>
    /// <seealso cref="::testing::TestWithParam{AuthnTestOptions}" />
    class Framework_RpcAuthn_TestCase :
        public ::testing::TestWithParam<AuthnTestOptions>
    {
        static void SetUpTestCase()
        {
            system("pause");
        }
    };

    /// <summary>
    /// Tests RPC client issuing requests for several scenarios of
    /// protocol sequence and authentication level.
    /// </summary>
    TEST_P(Framework_RpcAuthn_TestCase, ClientRun_AuthnSec_RequestTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            AcmeRpcClient client1(
                GetParam().protocolSequence,
                GetParam().objectUUID1,
                "TARS",//"MyVirtualServer.MyDomain.local",
                GetParam().authenticationSecurity,
                GetParam().authenticationLevel,
                GetParam().impersonationLevel,
                "Felipe@MyDomain.local"
            );

            EXPECT_EQ(696.0, client1.Operate(6.0, 116.0));
            EXPECT_EQ("SQUIRREL", client1.ChangeCase("squirrel"));

            AcmeRpcClient client2(
                GetParam().protocolSequence,
                GetParam().objectUUID2,
                "TARS",//"MyVirtualServer.MyDomain.local",
                GetParam().authenticationSecurity,
                GetParam().authenticationLevel,
                GetParam().impersonationLevel,
                "Felipe@MyDomain.local"
            );

            EXPECT_EQ(696.0, client2.Operate(606.0, 90.0));
            EXPECT_EQ("squirrel", client2.ChangeCase("SQUIRREL"));

            client2.WriteOnStorage();

            auto timeout = client2.Shutdown();

            std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        }
        catch (...)
        {
            HandleException();
        }
    }

    /* Implementation of test template takes care of switching
    protocol sequences and authentication level: */
    INSTANTIATE_TEST_CASE_P(
        SwitchProtAndAuthLevel,
        Framework_RpcAuthn_TestCase,
        ::testing::Values(
            AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[6], objectsUuidsImpl2[6], AuthenticationLevel::Integrity, AuthenticationSecurity::NTLM, ImpersonationLevel::Impersonate },
            AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[7], objectsUuidsImpl2[7], AuthenticationLevel::Privacy, AuthenticationSecurity::NTLM, ImpersonationLevel::Impersonate },
            AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[8], objectsUuidsImpl2[8], AuthenticationLevel::Integrity, AuthenticationSecurity::TryKerberos, ImpersonationLevel::Impersonate },
            AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[9], objectsUuidsImpl2[9], AuthenticationLevel::Privacy, AuthenticationSecurity::TryKerberos, ImpersonationLevel::Impersonate }
            /*,
            AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[10], objectsUuidsImpl2[10], AuthenticationLevel::Integrity, AuthenticationSecurity::RequireMutualAuthn, ImpersonationLevel::Impersonate },
            AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[11], objectsUuidsImpl2[11], AuthenticationLevel::Privacy, AuthenticationSecurity::RequireMutualAuthn, ImpersonationLevel::Impersonate }
            */
            /*,
            AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[5], objectsUuidsImpl2[5], AuthenticationLevel::Integrity, AuthenticationSecurity::NTLM, ImpersonationLevel::Impersonate },
            AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[6], objectsUuidsImpl2[6], AuthenticationLevel::Privacy, AuthenticationSecurity::NTLM, ImpersonationLevel::Impersonate },
            AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[7], objectsUuidsImpl2[7], AuthenticationLevel::Integrity, AuthenticationSecurity::TryKerberos, ImpersonationLevel::Impersonate },
            AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[8], objectsUuidsImpl2[8], AuthenticationLevel::Privacy, AuthenticationSecurity::TryKerberos, ImpersonationLevel::Impersonate }
            */
            /*,
            AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[9], objectsUuidsImpl2[9], AuthenticationLevel::Integrity, AuthenticationSecurity::RequireMutualAuthn, ImpersonationLevel::Impersonate },
            AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[10], objectsUuidsImpl2[10], AuthenticationLevel::Privacy, AuthenticationSecurity::RequireMutualAuthn, ImpersonationLevel::Impersonate }
            */
        )
    );

    // The set of options for each test template instantiation
    struct SchannelTestOptions
    {
        const char *objectUUID1;
        const char *objectUUID2;
        AuthenticationLevel authenticationLevel;
        bool useStrongSec;
    };

    /// <summary>
    /// Test case for RPC client with SCHANNEL authentication.
    /// </summary>
    /// <seealso cref="::testing::TestWithParam{SchannelTestOptions}" />
    class Framework_RpcSchannel_TestCase :
        public ::testing::TestWithParam<SchannelTestOptions>
    {
        static void SetUpTestCase()
        {
            system("pause");
        }
    };

    /// <summary>
    /// Tests RPC client issuing requests for several scenarios of
    /// protocol sequence and authentication level using Schannel SSP.
    /// </summary>
    TEST_P(Framework_RpcSchannel_TestCase, ClientRun_RequestTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            CertInfo certInfo(
                CERT_SYSTEM_STORE_LOCAL_MACHINE,
                "My",
                "MySelfSignedCert4DevTestsClient",
                GetParam().useStrongSec
            );

            AcmeRpcClient client1(
                GetParam().objectUUID1,
                "TARS",//"MyVirtualServer.MyDomain.local",
                certInfo,
                GetParam().authenticationLevel
            );

            EXPECT_EQ(696.0, client1.Operate(6.0, 116.0));
            EXPECT_EQ("SQUIRREL", client1.ChangeCase("squirrel"));

            AcmeRpcClient client2(
                GetParam().objectUUID2,
                "TARS",//"MyVirtualServer.MyDomain.local",
                certInfo,
                GetParam().authenticationLevel
            );

            EXPECT_EQ(696.0, client2.Operate(606.0, 90.0));
            EXPECT_EQ("squirrel", client2.ChangeCase("SQUIRREL"));

            auto timeout = client2.Shutdown();

            std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        }
        catch (...)
        {
            HandleException();
        }
    }

    /* Implementation of test template takes care of switching
    protocol sequences and authentication level: */
    INSTANTIATE_TEST_CASE_P(
        SwitchProtAndAuthLevel,
        Framework_RpcSchannel_TestCase,
        ::testing::Values(
            SchannelTestOptions{ objectsUuidsImpl1[12], objectsUuidsImpl2[12], AuthenticationLevel::Privacy, false },
            SchannelTestOptions{ objectsUuidsImpl1[13], objectsUuidsImpl2[13], AuthenticationLevel::Privacy, true },
            SchannelTestOptions{ objectsUuidsImpl1[14], objectsUuidsImpl2[14], AuthenticationLevel::Integrity, false },
            SchannelTestOptions{ objectsUuidsImpl1[15], objectsUuidsImpl2[15], AuthenticationLevel::Integrity, true }
        )
    );

}// end of namespace integration_tests
}// end of namespace _3fd
