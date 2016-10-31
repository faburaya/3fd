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
#include <cstring>
#include <wincrypt.h>

//////////////////////////////////////
// RPC Server Stubs Implementation
//////////////////////////////////////

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

    ImpersonateClientAndCreateFile(IDL_handle);
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

    ImpersonateClientAndCreateFile(IDL_handle);
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

    ImpersonateClientAndCreateFile(IDL_handle);
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

    ImpersonateClientAndCreateFile(IDL_handle);
}

// Common shutdown procedure
void Shutdown(
    /* [in] */ handle_t IDL_handle)
{
    RpcMgmtStopServerListening(nullptr);
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
    /// Tests the cycle init/start/stop/resume/stop/finalize of the RPC server,
    /// for local RPC and without authentication security.
    /// </summary>
    TEST(Framework_RPC_TestCase1, ServerRun_NoAuth_StatesCycleTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            // Initialize the RPC server (resource allocation takes place)
            RpcServer::Initialize(ProtocolSequence::Local, "TestClient3FD");

            // RPC interface implementation 1:
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, Shutdown };

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
            EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Resume());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());

            // Finalize the RPC server (resources will be released)
            RpcServer::Finalize();
        }
        catch (...)
        {
            RpcServer::Finalize();
            HandleException();
        }
    }

    // The set of options for some test template instantiations
    struct TestOptionsA
    {
        ProtocolSequence protocolSequence;
        const char *objectUUID1;
        const char *objectUUID2;
        AuthenticationLevel authenticationLevel;
    };

    class Framework_RPC_TestCase2 :
        public ::testing::TestWithParam<TestOptionsA> {};

    /// <summary>
    /// Tests the cycle init/start/stop/resume/stop/finalize of the RPC server,
    /// for several combinations of protocol sequence and authentication level.
    /// </summary>
    TEST_P(Framework_RPC_TestCase2, ServerRun_AuthnSec_StatesCycleTest)
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
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, Shutdown };

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
            EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Resume());
            EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());

            // Finalize the RPC server (resources will be released)
            RpcServer::Finalize();
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
        Framework_RPC_TestCase2,
        ::testing::Values(
            TestOptionsA { ProtocolSequence::Local, objectsUuidsImpl1[1], objectsUuidsImpl2[1], AuthenticationLevel::Integrity },
            TestOptionsA { ProtocolSequence::Local, objectsUuidsImpl1[2], objectsUuidsImpl2[2], AuthenticationLevel::Privacy },
            TestOptionsA { ProtocolSequence::TCP, objectsUuidsImpl1[3], objectsUuidsImpl2[3], AuthenticationLevel::Integrity },
            TestOptionsA { ProtocolSequence::TCP, objectsUuidsImpl1[4], objectsUuidsImpl2[4], AuthenticationLevel::Privacy }
        )
    );

    /// <summary>
    /// Tests the RPC server normal operation (responding requests), trying
    /// several combinations of protocol sequence and authentication level.
    /// </summary>
    TEST(Framework_RPC_TestCase3, ServerRun_NoAuth_ResponseTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            // Initialize the RPC server (resource allocation takes place)
            RpcServer::Initialize(ProtocolSequence::Local, "TestClient3FD");

            // RPC interface implementation 1:
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, Shutdown };

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

            // Now cycle through the states:
            EXPECT_EQ(STATUS_OKAY, RpcServer::Start(objects));
            EXPECT_EQ(STATUS_OKAY, RpcServer::Wait());

            // Finalize the RPC server (resources will be released)
            RpcServer::Finalize();
        }
        catch (...)
        {
            RpcServer::Finalize();
            HandleException();
        }
    }

    class Framework_RPC_TestCase4 :
        public ::testing::TestWithParam<TestOptionsA> {};

    /// <summary>
    /// Tests the RPC server normal operation (responding requests), trying
    /// several combinations of protocol sequence and authentication level
    /// using Microsoft NTLM/Negotiate/Kerberos SSP's.
    /// </summary>
    TEST_P(Framework_RPC_TestCase4, ServerRun_AuthnSec_ResponseTest)
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
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, Shutdown };

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
            EXPECT_EQ(STATUS_OKAY, RpcServer::Wait());

            // Finalize the RPC server (resources will be released)
            RpcServer::Finalize();
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
        Framework_RPC_TestCase4,
        ::testing::Values(
            TestOptionsA{ ProtocolSequence::Local, objectsUuidsImpl1[6], objectsUuidsImpl2[6], AuthenticationLevel::Integrity },
            TestOptionsA{ ProtocolSequence::Local, objectsUuidsImpl1[7], objectsUuidsImpl2[7], AuthenticationLevel::Privacy },
            TestOptionsA{ ProtocolSequence::Local, objectsUuidsImpl1[8], objectsUuidsImpl2[8], AuthenticationLevel::Integrity },
            TestOptionsA{ ProtocolSequence::Local, objectsUuidsImpl1[9], objectsUuidsImpl2[9], AuthenticationLevel::Privacy }
            /*,
            TestOptionsA{ ProtocolSequence::Local, objectsUuidsImpl1[10], objectsUuidsImpl2[10], AuthenticationLevel::Integrity },
            TestOptionsA{ ProtocolSequence::Local, objectsUuidsImpl1[11], objectsUuidsImpl2[11], AuthenticationLevel::Privacy },
            */
            /*
            TestOptionsA{ ProtocolSequence::TCP, objectsUuidsImpl1[6], objectsUuidsImpl2[6], AuthenticationLevel::Integrity },
            TestOptionsA{ ProtocolSequence::TCP, objectsUuidsImpl1[7], objectsUuidsImpl2[7], AuthenticationLevel::Privacy },
            TestOptionsA{ ProtocolSequence::TCP, objectsUuidsImpl1[8], objectsUuidsImpl2[8], AuthenticationLevel::Integrity },
            TestOptionsA{ ProtocolSequence::TCP, objectsUuidsImpl1[9], objectsUuidsImpl2[9], AuthenticationLevel::Privacy }
            */
            /*,
            TestOptionsA{ ProtocolSequence::TCP, objectsUuidsImpl1[10], objectsUuidsImpl2[10], AuthenticationLevel::Integrity },
            TestOptionsA{ ProtocolSequence::TCP, objectsUuidsImpl1[11], objectsUuidsImpl2[11], AuthenticationLevel::Privacy }
            */
        )
    );

    // The set of options for some test template instantiations
    struct TestOptionsB
    {
        ProtocolSequence protocolSequence;
        const char *objectUUID1;
        const char *objectUUID2;
        AuthenticationLevel authenticationLevel;
        bool useStrongSec;
    };

    class Framework_RPC_TestCase5 :
        public ::testing::TestWithParam<TestOptionsB> {};

    /// <summary>
    /// Tests the RPC server normal operation (responding requests), trying
    /// several combinations of protocol sequence and authentication level
    /// using Schannel SSP.
    /// </summary>
    TEST_P(Framework_RPC_TestCase5, ServerRun_Schannel_ResponseTest)
    {
        // Ensures proper initialization/finalization of the framework
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            CertInfo certInfo(
                CERT_SYSTEM_STORE_LOCAL_MACHINE,
                "My",
                "TARS",
                GetParam().useStrongSec
            );

            // Initialize the RPC server (resource allocation takes place)
            RpcServer::Initialize(
                GetParam().protocolSequence,
                "TestClient3FD",
                &certInfo,
                GetParam().authenticationLevel
            );

            // RPC interface implementation 1:
            AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, Shutdown };

            // RPC interface implementation 2:
            AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, Shutdown };

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
            EXPECT_EQ(STATUS_OKAY, RpcServer::Wait());

            // Finalize the RPC server (resources will be released)
            RpcServer::Finalize();
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
        Framework_RPC_TestCase5,
        ::testing::Values(
            /*TestOptionsB{ ProtocolSequence::Local, objectsUuidsImpl1[12], objectsUuidsImpl2[12], AuthenticationLevel::Integrity, false },
            TestOptionsB{ ProtocolSequence::Local, objectsUuidsImpl1[13], objectsUuidsImpl2[13], AuthenticationLevel::Integrity, true },
            TestOptionsB{ ProtocolSequence::Local, objectsUuidsImpl1[14], objectsUuidsImpl2[14], AuthenticationLevel::Privacy, false },
            TestOptionsB{ ProtocolSequence::Local, objectsUuidsImpl1[15], objectsUuidsImpl2[15], AuthenticationLevel::Privacy, true },*/
            TestOptionsB{ ProtocolSequence::TCP, objectsUuidsImpl1[16], objectsUuidsImpl2[16], AuthenticationLevel::Integrity, false },
            TestOptionsB{ ProtocolSequence::TCP, objectsUuidsImpl1[17], objectsUuidsImpl2[17], AuthenticationLevel::Integrity, true },
            TestOptionsB{ ProtocolSequence::TCP, objectsUuidsImpl1[18], objectsUuidsImpl2[18], AuthenticationLevel::Privacy, false },
            TestOptionsB{ ProtocolSequence::TCP, objectsUuidsImpl1[19], objectsUuidsImpl2[19], AuthenticationLevel::Privacy, true }
        )
    );
}// end of namespace integration_tests
}// end of namespace _3fd
