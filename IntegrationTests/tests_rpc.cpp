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

#include <vector>
#include <fstream>
#include <chrono>
#include <cstring>
#include <wincrypt.h>

/// <summary>
/// Impersonates the client and creates a file.
/// </summary>
/// <param name="clientBindingHandle">The client binding handle.</param>
static void ImpersonateClientAndCreateFile(handle_t clientBindingHandle)
{
    CALL_STACK_TRACE;

    using namespace _3fd::core;

    try
    {
        _3fd::rpc::ScopedImpersonation scopedCliImp(clientBindingHandle);

        std::ofstream outFileStream("createdByRpcServerProc.txt", std::ios::trunc);

        if (!outFileStream.is_open())
        {
            throw AppException<std::runtime_error>(
                "Implementation of RPC server procedure could not create file as impersonated client"
            );
        }

        outFileStream << "This file has been created by RPC server procedure impersonated as the client." << std::flush;

        if (outFileStream.bad())
        {
            throw AppException<std::runtime_error>(
                "Implementation of RPC server procedure could not write to file as impersonated client"
            );
        }
    }
    catch (IAppException &appEx)
    {
        Logger::Write(appEx, Logger::PRIO_ERROR);
    }
    catch (std::exception &ex)
    {
        std::ostringstream oss;
        oss << "Generic failure in RPC server procedure: " << ex.what();
        Logger::Write(oss.str(), Logger::PRIO_ERROR, true);
    }
}

// 1st implementation for 'Operate'
void Operate(
    /* [in] */ handle_t IDL_handle,
    /* [in] */ double left,
    /* [in] */ double right,
    /* [out] */ double *result)
{
    CALL_STACK_TRACE;

    *result = left * right;
}

// 2nd implementation for 'Operate'
void Operate2(
    /* [in] */ handle_t IDL_handle,
    /* [in] */ double left,
    /* [in] */ double right,
    /* [out] */ double *result)
{
    CALL_STACK_TRACE;

    *result = left + right;
}

// 1st implementation for 'ChangeCase'
void ChangeCase(
    /* [in] */ handle_t IDL_handle,
    /* [in] */ cstring *input,
    /* [out] */ cstring *output)
{
    CALL_STACK_TRACE;

    /* When the stubs have been generated for OSF compliance, RpcSs/RpcSm procs
    are to be used for dynamic allocation, instead of midl_user_allocate/free. This
    memory gets automatically released once this RPC returns to the caller. */

    output->data = static_cast<unsigned char *> (RpcSsAllocate(input->size));
    //output->data = static_cast<unsigned char *> (midl_user_allocate(input->size));
    output->size = input->size;

    const auto length = input->size - 1;
    for (unsigned short idx = 0; idx < length; ++idx)
        output->data[idx] = toupper(input->data[idx]);

    output->data[length] = 0;
}

// 2nd implementation for 'ChangeCase'
void ChangeCase2(
    /* [in] */ handle_t IDL_handle,
    /* [in] */ cstring *input,
    /* [out] */ cstring *output)
{
    CALL_STACK_TRACE;

    /* When the stubs have been generated for OSF compliance, RpcSs/RpcSm procs
    are to be used for dynamic allocation, instead of midl_user_allocate/free. This
    memory gets automatically released once this RPC returns to the caller. */

    output->data = static_cast<unsigned char *> (RpcSsAllocate(input->size));
    //output->data = static_cast<unsigned char *> (midl_user_allocate(input->size));
    output->size = input->size;

    const auto length = input->size - 1;
    for (unsigned short idx = 0; idx < length; ++idx)
        output->data[idx] = tolower(input->data[idx]);

    output->data[length] = 0;
}

// Common procedure that writes a file in server storage in order to test impersonation
void WriteOnStorage(
    /* [in] */ handle_t IDL_handle)
{
    ImpersonateClientAndCreateFile(IDL_handle);
}

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
    /// Basic test timer for the RPC module.
    /// This implements simple timing for RPC server cycles.
    /// </summary>
    class RpcTestTimer
    {
    private:

        static std::chrono::time_point<std::chrono::system_clock> startTimeForSrvSetupAndStart;
        static std::chrono::time_point<std::chrono::system_clock> startTimeForSrvShutdown;

        static std::chrono::milliseconds maxTimeSpanForSrvSetupAndStart;
        static std::chrono::milliseconds maxTimeSpanForSrvShutdown;

    public:

        /// <summary>
        /// Starts counting time for RPC server setup and start.
        /// </summary>
        static void StartTimeCountServerSetupAndStart()
        {
            startTimeForSrvSetupAndStart = std::chrono::system_clock().now();
        }

        /// <summary>
        /// Stops counting time for RPC server setup and start.
        /// </summary>
        static void StopTimeCountServerSetupAndStart()
        {
            using namespace std::chrono;

            auto now = system_clock().now();

            auto setupAndStartTimeSpan = duration_cast<milliseconds>(
                now - startTimeForSrvSetupAndStart
            );

            if (setupAndStartTimeSpan > maxTimeSpanForSrvSetupAndStart)
                maxTimeSpanForSrvSetupAndStart = setupAndStartTimeSpan;
        }

        /// <summary>
        /// Starts counting time for RPC server shutdown.
        /// </summary>
        static void StartTimeCountServerShutdown()
        {
            startTimeForSrvShutdown = std::chrono::system_clock().now();
        }

        /// <summary>
        /// Stops counting time for RPC server shutdown.
        /// </summary>
        static void StopTimeCountServerShutdown()
        {
            using namespace std::chrono;

            auto now = system_clock().now();

            auto shutdownTimeSpan = duration_cast<milliseconds>(
                now - startTimeForSrvShutdown
            );

            if (shutdownTimeSpan > maxTimeSpanForSrvShutdown)
                maxTimeSpanForSrvShutdown = shutdownTimeSpan;

            auto maxCycleTime = static_cast<uint32_t> (
                maxTimeSpanForSrvShutdown.count()
                + maxTimeSpanForSrvSetupAndStart.count()
            );

            std::ostringstream oss;
            oss << "Max registered time span for RPC server cycle shutdown-setup-start is "
                << maxCycleTime << " ms";

            Logger::Write(oss.str(), Logger::PRIO_NOTICE);
        }

        /// <summary>
        /// Retrieves the maximum cycle time for
        /// the RPC server registered so far.
        /// </summary>
        /// <return>
        /// The maximum cycle time in milliseconds.
        /// </return>
        static uint32_t GetEstimateCycleTime()
        {
            auto maxCycleTime =
                maxTimeSpanForSrvShutdown.count() +
                maxTimeSpanForSrvSetupAndStart.count();

            /* In practice, measured time must be linearly augmented
            for adjustment (using field data), because apparently the
            server takes longer to be available, which is a little
            after RpcServer::Start returns... */
            return static_cast<uint32_t> (300 + maxCycleTime);
        }
    };

    std::chrono::time_point<std::chrono::system_clock> RpcTestTimer::startTimeForSrvSetupAndStart;
    std::chrono::time_point<std::chrono::system_clock> RpcTestTimer::startTimeForSrvShutdown;

    std::chrono::milliseconds RpcTestTimer::maxTimeSpanForSrvSetupAndStart(0);
    std::chrono::milliseconds RpcTestTimer::maxTimeSpanForSrvShutdown(0);

    /// <summary>
    /// Tests the cycle init/start/stop/resume/stop/finalize of the RPC server,
    /// for local RPC and without authentication security.
    /// </summary>
    TEST(Framework_RpcNoAuth_TestCase, ServerRun_StatesCycleTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            RpcTestTimer::StartTimeCountServerSetupAndStart();

            // Initialize the RPC server (resource allocation takes place)
            RpcServer::Initialize(ProtocolSequence::Local, "TestClient3FD");

            // RPC interface implementation 1:
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, WriteOnStorage, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, WriteOnStorage, Shutdown };

            std::vector<RpcSrvObject> objects;
            objects.reserve(2);

            // This object will run impl 1:
            objects.emplace_back(
                objectsUuidsImpl1[0],
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable1
            );

            // This object will run impl 2:
            objects.emplace_back(
                objectsUuidsImpl2[0],
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable2
            );

            // Now cycle through the states:

            EXPECT_EQ(STATUS_OKAY, RpcServer::Start(objects));
            RpcTestTimer::StopTimeCountServerSetupAndStart();

            EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Resume());

            RpcTestTimer::StartTimeCountServerShutdown();

            // Upon finalization (shutdown), resources will be released:
            EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Finalize());

            RpcTestTimer::StopTimeCountServerShutdown();
        }
        catch (...)
        {
            RpcServer::Finalize();
            HandleException();
        }
    }

    class Framework_RpcNoAuth2_TestCase : public ::testing::Test {};

    /// <summary>
    /// Tests the RPC server normal operation (responding requests), trying
    /// several combinations of protocol sequence and authentication level.
    /// </summary>
    TEST_F(Framework_RpcNoAuth2_TestCase, ServerRun_ResponseTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            // Initialize the RPC server (resource allocation takes place)
            RpcServer::Initialize(ProtocolSequence::Local, "TestClient3FD");

            // RPC interface implementation 1:
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, WriteOnStorage, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, WriteOnStorage, Shutdown };

            std::vector<RpcSrvObject> objects;
            objects.reserve(2);

            // This object will run impl 1:
            objects.emplace_back(
                objectsUuidsImpl1[5],
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable1
            );

            // This object will run impl 2:
            objects.emplace_back(
                objectsUuidsImpl2[5],
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable2
            );

            EXPECT_EQ(STATUS_OKAY, RpcServer::Start(objects));
            EXPECT_EQ(STATUS_OKAY, RpcServer::Wait());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Finalize());
        }
        catch (...)
        {
            RpcServer::Finalize();
            HandleException();
        }
    }

    // The set of options for some test template instantiations
    struct AuthnTestOptions
    {
        ProtocolSequence protocolSequence;
        const char *objectUUID1;
        const char *objectUUID2;
        AuthenticationLevel authenticationLevel;
    };

    class Framework_RpcAuthn_TestCase :
        public ::testing::TestWithParam<AuthnTestOptions> {};

    /// <summary>
    /// Tests the cycle init/start/stop/resume/stop/finalize of the RPC server,
    /// for several combinations of protocol sequence and authentication level.
    /// </summary>
    TEST_P(Framework_RpcAuthn_TestCase, ServerRun_StatesCycleTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            RpcTestTimer::StartTimeCountServerSetupAndStart();

            // Initialize the RPC server (authn svc reg & resource allocation takes place)
            RpcServer::Initialize(
                GetParam().protocolSequence,
                "TestClient3FD",
                GetParam().authenticationLevel
            );

            // RPC interface implementation 1:
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, WriteOnStorage, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, WriteOnStorage, Shutdown };

            std::vector<RpcSrvObject> objects;
            objects.reserve(2);

            // This object will run impl 1:
            objects.emplace_back(
                GetParam().objectUUID1,
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable1
            );
                
            // This object will run impl 2:
            objects.emplace_back(
                GetParam().objectUUID2,
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable2
            );

            // Now cycle through the states:

            EXPECT_EQ(STATUS_OKAY, RpcServer::Start(objects));
            RpcTestTimer::StopTimeCountServerSetupAndStart();

            EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Resume());

            RpcTestTimer::StartTimeCountServerShutdown();

            // Upon finalization (shutdown), resources will be released:
            EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Finalize());

            RpcTestTimer::StopTimeCountServerShutdown();
        }
        catch (...)
        {
            RpcServer::Finalize();
            HandleException();
        }
    }

    /* Implementation of test template takes care of switching
       protocol sequences and authentication level: */
    INSTANTIATE_TEST_CASE_P(
        SwitchProtAndAuthLevel,
        Framework_RpcAuthn_TestCase,
        ::testing::Values(
            AuthnTestOptions { ProtocolSequence::Local, objectsUuidsImpl1[1], objectsUuidsImpl2[1], AuthenticationLevel::Integrity },
            AuthnTestOptions { ProtocolSequence::Local, objectsUuidsImpl1[2], objectsUuidsImpl2[2], AuthenticationLevel::Privacy },
            AuthnTestOptions { ProtocolSequence::TCP, objectsUuidsImpl1[3], objectsUuidsImpl2[3], AuthenticationLevel::Integrity },
            AuthnTestOptions { ProtocolSequence::TCP, objectsUuidsImpl1[4], objectsUuidsImpl2[4], AuthenticationLevel::Privacy }
        )
    );

    class Framework_RpcAuthn2_TestCase :
        public ::testing::TestWithParam<AuthnTestOptions> {};

    /// <summary>
    /// Tests the RPC server normal operation (responding requests), trying
    /// several combinations of protocol sequence and authentication level
    /// using Microsoft NTLM/Negotiate/Kerberos SSP's.
    /// </summary>
    TEST_P(Framework_RpcAuthn2_TestCase, ServerRun_ResponseTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            // Initialize the RPC server (authn svc reg & resource allocation takes place)
            RpcServer::Initialize(
                GetParam().protocolSequence,
                "TestClient3FD",
                GetParam().authenticationLevel
            );

            // RPC interface implementation 1:
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, WriteOnStorage, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, WriteOnStorage, Shutdown };

            std::vector<RpcSrvObject> objects;
            objects.reserve(2);

            // This object will run impl 1:
            objects.emplace_back(
                GetParam().objectUUID1,
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable1
            );

            // This object will run impl 2:
            objects.emplace_back(
                GetParam().objectUUID2,
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable2
            );

            EXPECT_EQ(STATUS_OKAY, RpcServer::Start(objects));
            EXPECT_EQ(STATUS_OKAY, RpcServer::Wait());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Finalize());
        }
        catch (...)
        {
            RpcServer::Finalize();
            HandleException();
        }
    }

    auto testOptions = ::testing::Values(
#ifdef SCENARIO_SINGLE_BOX_LOCAL_SEC
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[6], objectsUuidsImpl2[6], AuthenticationLevel::Integrity },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[7], objectsUuidsImpl2[7], AuthenticationLevel::Privacy },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[8], objectsUuidsImpl2[8], AuthenticationLevel::Integrity },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[9], objectsUuidsImpl2[9], AuthenticationLevel::Privacy }
#elif defined SCENARIO_SINGLE_BOX_AD_SEC
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[6], objectsUuidsImpl2[6], AuthenticationLevel::Integrity },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[7], objectsUuidsImpl2[7], AuthenticationLevel::Privacy },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[8], objectsUuidsImpl2[8], AuthenticationLevel::Integrity },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[9], objectsUuidsImpl2[9], AuthenticationLevel::Privacy },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[10], objectsUuidsImpl2[10], AuthenticationLevel::Integrity },
        AuthnTestOptions{ ProtocolSequence::Local, objectsUuidsImpl1[11], objectsUuidsImpl2[11], AuthenticationLevel::Privacy }
#elif defined SCENARIO_REMOTE_WITH_AD_SEC
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[6], objectsUuidsImpl2[6], AuthenticationLevel::Integrity },
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[7], objectsUuidsImpl2[7], AuthenticationLevel::Privacy },
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[8], objectsUuidsImpl2[8], AuthenticationLevel::Integrity },
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[9], objectsUuidsImpl2[9], AuthenticationLevel::Privacy },
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[10], objectsUuidsImpl2[10], AuthenticationLevel::Integrity },
        AuthnTestOptions{ ProtocolSequence::TCP, objectsUuidsImpl1[11], objectsUuidsImpl2[11], AuthenticationLevel::Privacy }
#endif
    );

    /* Implementation of test template takes care of switching
    protocol sequences and authentication level: */
    INSTANTIATE_TEST_CASE_P(
        SwitchProtAndAuthLevel,
        Framework_RpcAuthn2_TestCase,
        testOptions
    );

    class Framework_RpcSchannel_TestCase :
        public ::testing::TestWithParam<SchannelTestOptions> {};

    /// <summary>
    /// Tests the cycle init/start/stop/resume/stop/finalize of the RPC server,
    /// for several combinations of protocol sequence and authentication level
    /// using Schannel SSP.
    /// </summary>
    TEST_P(Framework_RpcSchannel_TestCase, ServerRun_StatesCycleTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            RpcTestTimer::StartTimeCountServerSetupAndStart();

            CertInfo certInfo(
                CERT_SYSTEM_STORE_LOCAL_MACHINE,
                "My",
                "MySelfSignedCert4DevTestsServer",
                GetParam().useStrongSec
            );

            // Initialize the RPC server (resource allocation takes place)
            RpcServer::Initialize(
                "TestClient3FD",
                &certInfo,
                GetParam().authenticationLevel
            );

            // RPC interface implementation 1:
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, WriteOnStorage, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, WriteOnStorage, Shutdown };

            std::vector<RpcSrvObject> objects;
            objects.reserve(2);

            // This object will run impl 1:
            objects.emplace_back(
                GetParam().objectUUID1,
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable1
            );

            // This object will run impl 2:
            objects.emplace_back(
                GetParam().objectUUID2,
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable2
            );

            // Now cycle through the states:

            EXPECT_EQ(STATUS_OKAY, RpcServer::Start(objects));
            RpcTestTimer::StopTimeCountServerSetupAndStart();

            EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Resume());

            RpcTestTimer::StartTimeCountServerShutdown();

            // Upon finalization (shutdown), resources will be released:
            EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Finalize());

            RpcTestTimer::StopTimeCountServerShutdown();
        }
        catch (...)
        {
            RpcServer::Finalize();
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

    class Framework_RpcSchannel2_TestCase :
        public ::testing::TestWithParam<SchannelTestOptions> {};

    /// <summary>
    /// Tests the RPC server normal operation (responding requests), trying
    /// several combinations of protocol sequence and authentication level
    /// using Schannel SSP.
    /// </summary>
    TEST_P(Framework_RpcSchannel2_TestCase, ServerRun_ResponseTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            CertInfo certInfo(
                CERT_SYSTEM_STORE_LOCAL_MACHINE,
                "My",
                "MySelfSignedCert4DevTestsServer",
                GetParam().useStrongSec
            );

            // Initialize the RPC server (resource allocation takes place)
            RpcServer::Initialize(
                "TestClient3FD",
                &certInfo,
                GetParam().authenticationLevel
            );

            // RPC interface implementation 1:
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, WriteOnStorage, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, WriteOnStorage, Shutdown };

            std::vector<RpcSrvObject> objects;
            objects.reserve(2);

            // This object will run impl 1:
            objects.emplace_back(
                GetParam().objectUUID1,
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable1
            );

            // This object will run impl 2:
            objects.emplace_back(
                GetParam().objectUUID2,
                AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                &intfImplFuncTable2
            );

            EXPECT_EQ(STATUS_OKAY, RpcServer::Start(objects));
            EXPECT_EQ(STATUS_OKAY, RpcServer::Wait());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Finalize());
        }
        catch (...)
        {
            RpcServer::Finalize();
            HandleException();
        }
    }

    // Implementation of test template takes care of switching parameters:
    INSTANTIATE_TEST_CASE_P(
        SwitchProtAndAuthLevel,
        Framework_RpcSchannel2_TestCase,
        paramsForSchannelTests // shared by both client & server side
    );

}// end of namespace integration_tests
}// end of namespace _3fd

// Common shutdown procedure
unsigned long ::Shutdown(
    /* [in] */ handle_t IDL_handle)
{
    RpcMgmtStopServerListening(nullptr); // emits signal to stop RPC server
    return _3fd::integration_tests::RpcTestTimer::GetEstimateCycleTime();
}
