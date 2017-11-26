#include "stdafx.h"
#include "runtime.h"
#include "callstacktracer.h"
#include "configuration.h"
#include "rpc_helpers.h"
#include "rpc_test_shared.h"

#ifdef _WIN64
#   include "AcmeTesting_x64.h"
#else
#   include "AcmeTesting_w32.h"
#endif

#include <thread>
#include <wincrypt.h>

#define UNDEF_SPN     "RPC SERVER SPN IS UNDEFINED"
#define UNDEF_SRV_LOC "RPC SERVER LOCATION IS UNDEFINED"

// Server Principal Name (normally the FQDN of the user running the RPC server)
const char *keyForSPN = "testRpcServerPrincipalName";

#if defined SCENARIO_SINGLE_BOX_LOCAL_SEC || defined SCENARIO_SINGLE_BOX_AD_SEC
const char *keyForServerLocation = "testRpcServerSingleBox"; // localhost
#elif defined SCENARIO_REMOTE_WITH_AD_SEC
const char *keyForServerLocation = "testRpcServerWithADSec"; // FQDN of RPC server
#endif

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


    /// <summary>
    /// Proxy for AcmeTesting RPC server.
    /// </summary>
    class AcmeRpcClient : public rpc::RpcClient
    {
    public:

        using RpcClient::RpcClient;

        // RPC Invert
        void Invert(pair &onePair)
        {
            Call("Invert", [&onePair](rpc_binding_handle_t bindHandle)
            {
                ::Invert(bindHandle, &onePair);
            });
        }

        // RPC Operate
        double Operate(double left, double right)
        {
            double result;

            Call("Operate", [left, right, &result](rpc_binding_handle_t bindHandle)
            {
                ::Operate(bindHandle, left, right, &result);
            });

            return result;
        }

        // RPC ChangeCase
        string ChangeCase(const char *text)
        {
            /* When the stubs have been generated for OSF compliance the
            output string parameter must fulfill the memory allocation
            of the buffer carrying the text: */

            std::array<unsigned char, 256> output;
            
            Call("ChangeCase", [text, &output](rpc_binding_handle_t bindHandle)
            {
                ::ChangeCase(bindHandle, (unsigned char *)text, output.data());
            });

            return string((char *)output.data());
        }

        // RPC WriteOnStorage
        void WriteOnStorage()
        {
            Call("WriteOnStorage", [](rpc_binding_handle_t bindHandle)
            {
                ::WriteOnStorage(bindHandle);
            });
        }

        // RPC Shutdown
        uint32_t Shutdown()
        {
            uint32_t timeout;

            Call("WriteOnStorage", [&timeout](rpc_binding_handle_t bindHandle)
            {
                timeout = ::Shutdown(bindHandle);
            });

            return timeout;
        }
    };

    /// <summary>
    /// Test case for RPC client without authentication.
    /// </summary>
    class Framework_RpcNoAuth_TestCase : public ::testing::Test
    {
    public:

        static void SetUpTestCase()
        {
            //system("pause"); // wait until RPC server becomes available
        }
    };

    /// <summary>
    /// Tests RPC client issuing requests without authentication.
    /// </summary>
    TEST_F(Framework_RpcNoAuth_TestCase, ClientRun_RequestTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        uint32_t timeout(0);

        try
        {
            AcmeRpcClient client1(
                ProtocolSequence::Local,
                objectsUuidsImpl1[5],
                AppConfig::GetSettings().application.GetString(keyForServerLocation, UNDEF_SRV_LOC)
            );

            pair expPair = { 2, 1 };
            pair myPair = { 1, 2 };
            client1.Invert(myPair);
            EXPECT_TRUE(memcmp(&expPair, &myPair, sizeof myPair) == 0);
            
            EXPECT_EQ(696.0, client1.Operate(6.0, 116.0));
            EXPECT_EQ("SQUIRREL", client1.ChangeCase("squirrel"));

            AcmeRpcClient client2(
                ProtocolSequence::Local,
                objectsUuidsImpl2[5],
                AppConfig::GetSettings().application.GetString(keyForServerLocation, UNDEF_SRV_LOC)
            );

            EXPECT_EQ(696.0, client2.Operate(606.0, 90.0));
            EXPECT_EQ("squirrel", client2.ChangeCase("SQUIRREL"));

            timeout = client2.Shutdown();
        }
        catch (...)
        {
            HandleException();
        }

        /* Awaits for the setup and start of the RPC server in the
        next test, using data measured in the server side... */
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
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
    public:

        static void SetUpTestCase()
        {
            //system("pause"); // wait until RPC server becomes available
        }
    };

    /// <summary>
    /// Tests RPC client issuing requests for several scenarios of
    /// protocol sequence and authentication level.
    /// </summary>
    TEST_P(Framework_RpcAuthn_TestCase, ClientRun_RequestTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        uint32_t timeout(0);

        try
        {
            AcmeRpcClient client1(
                GetParam().protocolSequence,
                GetParam().objectUUID1,
                AppConfig::GetSettings().application.GetString(keyForServerLocation, UNDEF_SRV_LOC),
                GetParam().authenticationSecurity,
                GetParam().authenticationLevel,
                GetParam().impersonationLevel,
                AppConfig::GetSettings().application.GetString(keyForSPN, UNDEF_SPN) // not used for NTLM
            );

            pair expPair = { 2, 1 };
            pair myPair = { 1, 2 };
            client1.Invert(myPair);
            EXPECT_TRUE(memcmp(&expPair, &myPair, sizeof myPair) == 0);

            EXPECT_EQ(696.0, client1.Operate(6.0, 116.0));
            EXPECT_EQ("SQUIRREL", client1.ChangeCase("squirrel"));

            AcmeRpcClient client2(
                GetParam().protocolSequence,
                GetParam().objectUUID2,
                AppConfig::GetSettings().application.GetString(keyForServerLocation, UNDEF_SRV_LOC),
                GetParam().authenticationSecurity,
                GetParam().authenticationLevel,
                GetParam().impersonationLevel,
                AppConfig::GetSettings().application.GetString(keyForSPN, UNDEF_SPN) // not used for NTLM
            );

            EXPECT_EQ(696.0, client2.Operate(606.0, 90.0));
            EXPECT_EQ("squirrel", client2.ChangeCase("SQUIRREL"));

            client2.WriteOnStorage();

            timeout = client2.Shutdown();
        }
        catch (...)
        {
            HandleException();
        }

        /* Awaits for the setup and start of the RPC server in the
        next test, using data measured in the server side... */
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
    }

    auto testOptions = ::testing::Values(
#ifdef SCENARIO_SINGLE_BOX_LOCAL_SEC
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[6], objectsUuidsImpl2[6], AuthenticationLevel::Integrity, AuthenticationSecurity::NTLM, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[7], objectsUuidsImpl2[7], AuthenticationLevel::Privacy, AuthenticationSecurity::NTLM, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[8], objectsUuidsImpl2[8], AuthenticationLevel::Integrity, AuthenticationSecurity::TryKerberos, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[9], objectsUuidsImpl2[9], AuthenticationLevel::Privacy, AuthenticationSecurity::TryKerberos, ImpersonationLevel::Impersonate }
#elif defined SCENARIO_SINGLE_BOX_AD_SEC
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[6], objectsUuidsImpl2[6], AuthenticationLevel::Integrity, AuthenticationSecurity::NTLM, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[7], objectsUuidsImpl2[7], AuthenticationLevel::Privacy, AuthenticationSecurity::NTLM, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[8], objectsUuidsImpl2[8], AuthenticationLevel::Integrity, AuthenticationSecurity::TryKerberos, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[9], objectsUuidsImpl2[9], AuthenticationLevel::Privacy, AuthenticationSecurity::TryKerberos, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[10], objectsUuidsImpl2[10], AuthenticationLevel::Integrity, AuthenticationSecurity::RequireMutualAuthn, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[11], objectsUuidsImpl2[11], AuthenticationLevel::Privacy, AuthenticationSecurity::RequireMutualAuthn, ImpersonationLevel::Impersonate }
#elif defined SCENARIO_REMOTE_WITH_AD_SEC
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[6], objectsUuidsImpl2[6], AuthenticationLevel::Integrity, AuthenticationSecurity::NTLM, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[7], objectsUuidsImpl2[7], AuthenticationLevel::Privacy, AuthenticationSecurity::NTLM, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[8], objectsUuidsImpl2[8], AuthenticationLevel::Integrity, AuthenticationSecurity::TryKerberos, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[9], objectsUuidsImpl2[9], AuthenticationLevel::Privacy, AuthenticationSecurity::TryKerberos, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[10], objectsUuidsImpl2[10], AuthenticationLevel::Integrity, AuthenticationSecurity::RequireMutualAuthn, ImpersonationLevel::Impersonate },
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[11], objectsUuidsImpl2[11], AuthenticationLevel::Privacy, AuthenticationSecurity::RequireMutualAuthn, ImpersonationLevel::Impersonate }
#endif
    );

    /* Implementation of test template takes care of switching
       protocol sequences and authentication level: */
    INSTANTIATE_TEST_CASE_P(
        SwitchProtAndAuthLevel,
        Framework_RpcAuthn_TestCase,
        testOptions
    );

    /// <summary>
    /// Test case for RPC client with SCHANNEL authentication.
    /// </summary>
    /// <seealso cref="::testing::TestWithParam{SchannelTestOptions}" />
    class Framework_RpcSchannel_TestCase :
        public ::testing::TestWithParam<SchannelTestOptions>
    {
    public:

        static void SetUpTestCase()
        {
            //system("pause"); // wait until RPC server becomes available
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

        uint32_t timeout(0);

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
                AppConfig::GetSettings().application.GetString(keyForServerLocation, UNDEF_SRV_LOC),
                certInfo,
                GetParam().authenticationLevel
            );

            pair expPair = { 2, 1 };
            pair myPair = { 1, 2 };
            client1.Invert(myPair);
            EXPECT_TRUE(memcmp(&expPair, &myPair, sizeof myPair) == 0);

            EXPECT_EQ(696.0, client1.Operate(6.0, 116.0));
            EXPECT_EQ("SQUIRREL", client1.ChangeCase("squirrel"));

            AcmeRpcClient client2(
                GetParam().objectUUID2,
                AppConfig::GetSettings().application.GetString(keyForServerLocation, UNDEF_SRV_LOC),
                certInfo,
                GetParam().authenticationLevel
            );

            EXPECT_EQ(696.0, client2.Operate(606.0, 90.0));
            EXPECT_EQ("squirrel", client2.ChangeCase("SQUIRREL"));

            timeout = client2.Shutdown();
        }
        catch (...)
        {
            HandleException();
        }

        /* Awaits for the setup and start of the RPC server in the
        next test, using data measured in the server side... */
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
    }

    // Implementation of test template takes care of switching parameters:
    INSTANTIATE_TEST_CASE_P(
        SwitchProtAndAuthLevel,
        Framework_RpcSchannel_TestCase,
        paramsForSchannelTests // shared by both client & server side
    );

}// end of namespace integration_tests
}// end of namespace _3fd
