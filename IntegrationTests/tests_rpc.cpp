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
#include <cstring>

//////////////////////////////////////
// RPC Server Stubs Implementation
//////////////////////////////////////

// 1st implementation for 'Operate'
void Operate(
    /* [in] */ handle_t IDL_handle,
    /* [in] */ double left,
    /* [in] */ double right,
    /* [out] */ double *result)
{
    *result = left * right;
}

// 2nd implementation for 'Operate'
void Operate2(
    /* [in] */ handle_t IDL_handle,
    /* [in] */ double left,
    /* [in] */ double right,
    /* [out] */ double *result)
{
    *result = left + right;
}

// 1st implementation for 'ChangeCase'
void ChangeCase(
    /* [in] */ handle_t IDL_handle,
    /* [in] */ cstring *input,
    /* [out] */ cstring *output)
{
    /* Because the stubs have been generated for OSF compliance, RpcSs/RpcSm procs
    are to be used for dynamic allocation, instead of midl_user_allocate/free. This
    memory gets automatically released once this RPC returns to the caller. */
    output->data = (unsigned char *)RpcSsAllocate(input->size);
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
    /* Because the stubs have been generated for OSF compliance, RpcSs/RpcSm procs
    are to be used for dynamic allocation, instead of midl_user_allocate/free. This
    memory gets automatically released once this RPC returns to the caller. */
    output->data = (unsigned char *)RpcSsAllocate(input->size);
    output->size = input->size;

    const auto length = input->size - 1;
    for (unsigned short idx = 0; idx < length; ++idx)
        output->data[idx] = tolower(input->data[idx]);

    output->data[length] = 0;
}

// Common shutdown procedure
void Shutdown(
    /* [in] */ handle_t IDL_handle)
{
    RpcMgmtStopServerListening(nullptr);
}

namespace _3fd
{
    namespace integration_tests
    {
        using namespace core;
        using namespace rpc;

        void HandleException();

        // The set of options for each test template instantiation
        struct TestOptions
        {
            ProtocolSequence protocolSequence;
            const char *objectUUID1;
            const char *objectUUID2;
            AuthenticationLevel authenticationLevel;
        };

        class Framework_RPC_TestCase :
            public ::testing::TestWithParam<TestOptions> {};

        /// <summary>
        /// Tests the cycle init/start/stop/resume/stop/finalize of the RPC server,
        /// for several combinations of protocol sequence and authentication level.
        /// </summary>
        TEST_P(Framework_RPC_TestCase, ServerRun_StatesCycleTest)
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
            Framework_RPC_TestCase,
            ::testing::Values(
                TestOptions {
                    ProtocolSequence::Local,
                    objectsUuidsImpl1[0],
                    objectsUuidsImpl2[0],
                    AuthenticationLevel::None
                },
                TestOptions {
                    ProtocolSequence::Local,
                    objectsUuidsImpl1[1],
                    objectsUuidsImpl2[1],
                    AuthenticationLevel::Integrity
                },
                TestOptions {
                    ProtocolSequence::Local,
                    objectsUuidsImpl1[2],
                    objectsUuidsImpl2[2],
                    AuthenticationLevel::Privacy
                },
                TestOptions {
                    ProtocolSequence::TCP,
                    objectsUuidsImpl1[3],
                    objectsUuidsImpl2[3],
                    AuthenticationLevel::None
                },
                TestOptions {
                    ProtocolSequence::TCP,
                    objectsUuidsImpl1[4],
                    objectsUuidsImpl2[4],
                    AuthenticationLevel::Integrity
                },
                TestOptions {
                    ProtocolSequence::TCP,
                    objectsUuidsImpl1[5],
                    objectsUuidsImpl2[5],
                    AuthenticationLevel::Privacy
                }
            )
        );

        /// <summary>
        /// Tests the cycle init/start/stop/resume/stop/finalize of the RPC server,
        /// for several combinations of protocol sequence and authentication level.
        /// </summary>
        TEST(Framework_RPC_TestCase, ServerRun_ResponseTest)
        {
            // Ensures proper initialization/finalization of the framework
            FrameworkInstance _framework;

            CALL_STACK_TRACE;

            try
            {
                // Initialize the RPC server (authn svc reg & resource allocation takes place)
                RpcServer::Initialize(
                    ProtocolSequence::TCP,
                    "TestClient3FD",
                    AuthenticationLevel::Integrity,
                    "ericsson.se"
                );

                // RPC interface implementation 1:
                AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, Shutdown };

                // RPC interface implementation 2:
                AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, Shutdown };

                std::vector<RpcSrvObject> objects;
                objects.reserve(2);

                // This object will run impl 1:
                objects.emplace_back(
                    objectsUuidsImpl1[6],
                    AcmeTesting_v1_0_s_ifspec, // this is the interface (generated from IDL)
                    &intfImplFuncTable1
                );

                // This object will run impl 2:
                objects.emplace_back(
                    objectsUuidsImpl2[6],
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

    }// end of namespace integration_tests
}// end of namespace _3fd
