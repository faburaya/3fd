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
        class AcmeSvcProxy : public rpc::RpcClient
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
                unsigned char buffer[128];
                cstring output { sizeof buffer, buffer };
                auto status = ChangeCaseImpl(GetBindingHandle(), text, output);
                ThrowIfError(status, "Failed to invoke RPC client stub routine \'ChangeCase\'");
                return string(output.data, output.data + output.size - 1);
            }

            void Shutdown()
            {
                ::Shutdown(GetBindingHandle());
            }
        };

        // The set of options for each test template instantiation
        struct TestOptions
        {
            ProtocolSequence protocolSequence;
            const char *objectUUID1;
            const char *objectUUID2;
            AuthenticationLevel authenticationLevel;
            AuthenticationSecurity authenticationSecurity;
        };

        class Framework_RPC_TestCase :
            public ::testing::TestWithParam<TestOptions> {};

        /// <summary>
        /// Tests RPC client issuing requests for several scenarios of
        /// protocol sequence and authentication level.
        /// </summary>
        TEST_P(Framework_RPC_TestCase, ClientRun_RequestTest)
        {
            // Ensures proper initialization/finalization of the framework
            FrameworkInstance _framework;

            CALL_STACK_TRACE;

            try
            {
                // Wait for RPC server to become available
                std::this_thread::sleep_for(std::chrono::seconds(1));

                AcmeSvcProxy client1(
                    GetParam().protocolSequence,
                    GetParam().objectUUID1,
                    "MyVirtualServer.MyDomain.local",
                    GetParam().authenticationLevel,
                    "Felipe@MyDomain.local",
                    GetParam().authenticationSecurity
                );

                EXPECT_EQ(696.0, client1.Operate(6.0, 116.0));
                EXPECT_EQ("SQUIRREL", client1.ChangeCase("squirrel"));

                AcmeSvcProxy client2(
                    GetParam().protocolSequence,
                    GetParam().objectUUID2,
                    "MyVirtualServer.MyDomain.local",
                    GetParam().authenticationLevel,
                    "Felipe@MyDomain.local",
                    GetParam().authenticationSecurity
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

        /* Implementation of test template takes care of switching
        protocol sequences and authentication level: */
        INSTANTIATE_TEST_CASE_P(
            SwitchProtAndAuthLevel,
            Framework_RPC_TestCase,
            ::testing::Values(/*
                TestOptions{
                    ProtocolSequence::Local,
                    objectsUuidsImpl1[6],
                    objectsUuidsImpl2[6],
                    AuthenticationLevel::None,
                    AuthenticationSecurity::NTLM // ignored
                },
                TestOptions{
                    ProtocolSequence::Local,
                    objectsUuidsImpl1[7],
                    objectsUuidsImpl2[7],
                    AuthenticationLevel::Integrity,
                    AuthenticationSecurity::NTLM
                },
                TestOptions{
                    ProtocolSequence::Local,
                    objectsUuidsImpl1[8],
                    objectsUuidsImpl2[8],
                    AuthenticationLevel::Privacy,
                    AuthenticationSecurity::NTLM
                },
                TestOptions{
                    ProtocolSequence::Local,
                    objectsUuidsImpl1[9],
                    objectsUuidsImpl2[9],
                    AuthenticationLevel::Integrity,
                    AuthenticationSecurity::TryKerberos
                },
                TestOptions{
                    ProtocolSequence::Local,
                    objectsUuidsImpl1[10],
                    objectsUuidsImpl2[10],
                    AuthenticationLevel::Privacy,
                    AuthenticationSecurity::TryKerberos
                },
                TestOptions{
                    ProtocolSequence::Local,
                    objectsUuidsImpl1[11],
                    objectsUuidsImpl2[11],
                    AuthenticationLevel::Integrity,
                    AuthenticationSecurity::RequireMutualAuthn
                },
                TestOptions{
                    ProtocolSequence::Local,
                    objectsUuidsImpl1[12],
                    objectsUuidsImpl2[12],
                    AuthenticationLevel::Privacy,
                    AuthenticationSecurity::RequireMutualAuthn
                },*/
                TestOptions{
                    ProtocolSequence::TCP,
                    objectsUuidsImpl1[6],
                    objectsUuidsImpl2[6],
                    AuthenticationLevel::Integrity,
                    AuthenticationSecurity::NTLM
                },
                TestOptions{
                    ProtocolSequence::TCP,
                    objectsUuidsImpl1[7],
                    objectsUuidsImpl2[7],
                    AuthenticationLevel::Privacy,
                    AuthenticationSecurity::NTLM
                },
                TestOptions{
                    ProtocolSequence::TCP,
                    objectsUuidsImpl1[8],
                    objectsUuidsImpl2[8],
                    AuthenticationLevel::Integrity,
                    AuthenticationSecurity::TryKerberos
                },
                TestOptions{
                    ProtocolSequence::TCP,
                    objectsUuidsImpl1[9],
                    objectsUuidsImpl2[9],
                    AuthenticationLevel::Privacy,
                    AuthenticationSecurity::TryKerberos
                },
                TestOptions{
                    ProtocolSequence::TCP,
                    objectsUuidsImpl1[10],
                    objectsUuidsImpl2[10],
                    AuthenticationLevel::Integrity,
                    AuthenticationSecurity::RequireMutualAuthn
                },
                TestOptions{
                    ProtocolSequence::TCP,
                    objectsUuidsImpl1[11],
                    objectsUuidsImpl2[11],
                    AuthenticationLevel::Privacy,
                    AuthenticationSecurity::RequireMutualAuthn
                }
            )
        );

    }// end of namespace integration_tests
}// end of namespace _3fd
