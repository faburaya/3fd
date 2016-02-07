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

void Multiply(
    /* [in] */ handle_t IDL_handle,
    /* [in] */ double left,
    /* [in] */ double right,
    /* [out] */ double *result)
{
    *result = left + right;
}

void ToUpperCase(
    /* [in] */ handle_t IDL_handle,
    /* [string][in] */ unsigned char *input,
    /* [out] */ cstring *output)
{
    unsigned short idx;
    for (idx = 0; input[idx] != 0; ++idx)
        input[idx] = toupper(input[idx]);

    output = new (RpcSsAllocate(sizeof (cstring))) cstring{ idx, input };
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
            const char *objectUUID;
            AuthenticationLevel authenticationLevel;

            TestOptions(ProtocolSequence p_protocolSequence,
                        const char *p_objectUUID,
                        AuthenticationLevel p_authenticationLevel) :
                protocolSequence(p_protocolSequence),
                objectUUID(p_objectUUID),
                authenticationLevel(p_authenticationLevel)
            {}
        };

        class Framework_RPC_TestCase :
            public ::testing::TestWithParam<TestOptions> {};

        /// <summary>
        /// Tests synchronous web service access without transport security.
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

                AcmeTesting_v1_0_epv_t defaultEPV;
                std::vector<RpcSrvObject> objects;
                objects.reserve(1);

                objects.emplace_back(
                    GetParam().objectUUID,
                    AcmeTesting_v1_0_s_ifspec,
                    &defaultEPV
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
            SwitchProtocols,
            Framework_RPC_TestCase,
            ::testing::Values(
                TestOptions(ProtocolSequence::Local, "BF7653EF-EB6D-4CF5-BEF0-B4D27864D750", AuthenticationLevel::None),
                TestOptions(ProtocolSequence::Local, "6642C890-14CF-4A9B-A35F-4C860A1DEBDE", AuthenticationLevel::Integrity),
                TestOptions(ProtocolSequence::Local, "6442FFD6-1688-4DF0-B2CD-E1633CE30027", AuthenticationLevel::Privacy),
                TestOptions(ProtocolSequence::TCP, "5C4E56F6-C92E-4268-BEFA-CC55A2FF833D", AuthenticationLevel::None),
                TestOptions(ProtocolSequence::TCP, "B3CCEEFD-990B-462D-B919-DAC45ACD8230", AuthenticationLevel::Integrity),
                TestOptions(ProtocolSequence::TCP, "D9D3881F-453F-48AC-A860-3B6AB31F8C6F", AuthenticationLevel::Privacy)
            )
        );

    }// end of namespace integration_tests
}// end of namespace _3fd
