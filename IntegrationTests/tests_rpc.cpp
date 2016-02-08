#include "stdafx.h"
#include "runtime.h"
#include "callstacktracer.h"
#include "rpc_helpers.h"

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

void Operate(
    /* [in] */ handle_t IDL_handle,
    /* [in] */ double left,
    /* [in] */ double right,
    /* [out] */ double *result)
{
    *result = left * right;
}

void Operate2(
    /* [in] */ handle_t IDL_handle,
    /* [in] */ double left,
    /* [in] */ double right,
    /* [out] */ double *result)
{
    *result = left + right;
}

void ChangeCase(
    /* [in] */ handle_t IDL_handle,
    /* [string][in] */ unsigned char *input,
    /* [out] */ cstring *output)
{
    unsigned short idx;
    for (idx = 0; input[idx] != 0; ++idx)
        input[idx] = toupper(input[idx]);

    /* Because the stubs have been generated for OSF compliance, RpcSs/RpcSm procs
    are to be used for dynamic allocation, instead of midl_user_allocate/free. This
    memory gets automatically released once this RPC returns to the caller. */
    output = new (RpcSsAllocate(sizeof (cstring))) cstring{ idx, input };
}

void ChangeCase2(
    /* [in] */ handle_t IDL_handle,
    /* [string][in] */ unsigned char *input,
    /* [out] */ cstring *output)
{
    unsigned short idx;
    for (idx = 0; input[idx] != 0; ++idx)
        input[idx] = tolower(input[idx]);

    /* Because the stubs have been generated for OSF compliance, RpcSs/RpcSm procs
    are to be used for dynamic allocation, instead of midl_user_allocate/free. This
    memory gets automatically released once this RPC returns to the caller. */
    output = new (RpcSsAllocate(sizeof(cstring))) cstring{ idx, input };
}

void Shutdown(
    /* [in] */ handle_t IDL_handle)
{
    RpcMgmtStopServerListening(IDL_handle);
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
        /// Tests the cycle init/start/stop/resume/start/stop/finalize of the RPC server,
        /// for several combinations of protocol sequence and authentication level.
        /// </summary>
        TEST_P(Framework_RPC_TestCase, ServerRun_StatesCycleTest)
        {
            // Ensures proper initialization/finalization of the framework
            FrameworkInstance _framework;

            CALL_STACK_TRACE;

            try
            {
                RpcServer::Initialize(
                    GetParam().protocolSequence,
                    "TestClient3FD",
                    GetParam().authenticationLevel,
                    false
                );

                AcmeTesting_v1_0_epv_t intfImplFuncTable1 = { Operate, ChangeCase, Shutdown };
                AcmeTesting_v1_0_epv_t intfImplFuncTable2 = { Operate2, ChangeCase2, Shutdown };

                std::vector<RpcSrvObject> objects;
                objects.reserve(2);

                objects.emplace_back(
                    GetParam().objectUUID1,
                    AcmeTesting_v1_0_s_ifspec,
                    &intfImplFuncTable1
                );
                
                objects.emplace_back(
                    GetParam().objectUUID2,
                    AcmeTesting_v1_0_s_ifspec,
                    &intfImplFuncTable2
                );

                EXPECT_EQ(STATUS_OKAY, RpcServer::Start(objects));
                EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());
                EXPECT_EQ(STATUS_OKAY, RpcServer::Resume());
                EXPECT_EQ(STATUS_OKAY, RpcServer::Stop());

                RpcServer::Finalize();
            }
            catch (...)
            {
                HandleException();
            }
        }

        INSTANTIATE_TEST_CASE_P(
            SwitchProtAndAuthLevel,
            Framework_RPC_TestCase,
            ::testing::Values(
                TestOptions {
                    ProtocolSequence::Local,
                    "BF7653EF-EB6D-4CF5-BEF0-B4D27864D750",
                    "684F4A12-FF51-4E7C-8611-CD4F54E5D042",
                    AuthenticationLevel::None
                },
                TestOptions {
                    ProtocolSequence::Local,
                    "6642C890-14CF-4A9B-A35F-4C860A1DEBDE",
                    "70A02E35-223C-47D4-8690-09AB0AF43B54",
                    AuthenticationLevel::Integrity
                },
                TestOptions {
                    ProtocolSequence::Local,
                    "6442FFD6-1688-4DF0-B2CD-E1633CE30027",
                    "22588C8D-12F4-49DE-BFA7-FD27E8AAF53D",
                    AuthenticationLevel::Privacy
                },
                TestOptions {
                    ProtocolSequence::TCP,
                    "5C4E56F6-C92E-4268-BEFA-CC55A2FF833D",
                    "94BBCAE5-E3C3-4899-BDE0-6889CBF441A2",
                    AuthenticationLevel::None
                },
                TestOptions {
                    ProtocolSequence::TCP,
                    "B3CCEEFD-990B-462D-B919-DAC45ACD8230",
                    "37277527-6CE7-4FC4-B4D2-E44676C2E256",
                    AuthenticationLevel::Integrity
                },
                TestOptions {
                    ProtocolSequence::TCP,
                    "D9D3881F-453F-48AC-A860-3B6AB31F8C6F",
                    "FA263BF8-C883-4478-B873-053077DF6093",
                    AuthenticationLevel::Privacy
                }
            )
        );

    }// end of namespace integration_tests
}// end of namespace _3fd
