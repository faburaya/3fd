#include "stdafx.h"
#include "runtime.h"
#include "utils.h"
#include "web_wws_webservicehost.h"
#include "calculator.wsdl.h"

#include <vector>
#include <chrono>

namespace _3fd
{
	namespace integration_tests
	{
		using namespace core;
		using namespace web::wws;

		void HandleException();

		/////////////////////////////
		// Web service operations
		/////////////////////////////

        HRESULT CALLBACK AddImpl(
            _In_ const WS_OPERATION_CONTEXT *wsContextHandle,
            _In_ double first,
            _In_ double second,
            _Out_ double *result,
            _In_ const WS_ASYNC_CONTEXT* asyncContext,
            _In_ WS_ERROR *wsErrorHandle
        );

        HRESULT CALLBACK MultiplyImpl(
            _In_ const WS_OPERATION_CONTEXT *wsContextHandle,
            _In_ double first,
            _In_ double second,
            _Out_ double *result,
            _In_ const WS_ASYNC_CONTEXT *asyncContext,
            _In_ WS_ERROR *wsErrorHandle
        );

        HRESULT CALLBACK CloseServiceImpl(
            _In_ const WS_OPERATION_CONTEXT *wsContextHandle,
            _Out_ __int64 *result,
            _In_ const WS_ASYNC_CONTEXT *asyncContext,
            _In_ WS_ERROR *wsErrorHandle
        );

        HRESULT CALLBACK Fail(
            _In_ const WS_OPERATION_CONTEXT* wsContextHandle,
            _In_ double first,
            _In_ double second,
            _Out_ double* result,
            _In_ const WS_ASYNC_CONTEXT* asyncContext,
            _In_ WS_ERROR* wsErrorHandle
        );

        /// <summary>
        /// Test fixture for the WWS module.
        /// </summary>
        class Framework_WWS_TestCase : public ::testing::Test
        {
        private:

            static std::unique_ptr<utils::Event> closeServiceRequestEvent;

            static std::chrono::milliseconds maxTimeSpanForSvcClosure;

        public:

            /// <summary>
            /// Signalize to close the web service host.
            /// </summary>
            static void SignalWebServiceClosureEvent()
            {
                closeServiceRequestEvent->Signalize();
            }

            /// <summary>
            /// Wait for signal to close the web service host.
            /// Once the signal is received, close it and measure how long that takes.
            /// The maximum closure time is kept for later use (of web clients).
            /// </summary>
            /// <param name="svc">The web service host object.</param>
            /// <returns>
            /// <c>true</c> when the signal was received and the host service
            /// succesfully closed, otherwise, <c>false</c>.
            /// </returns>
            bool WaitSignalAndClose(WebServiceHost &svc)
            {
                using namespace std::chrono;

                if (!closeServiceRequestEvent->WaitFor(8000))
                    return false;

                auto t1 = system_clock().now();

                if (!svc.Close())
                    return false;

                auto t2 = system_clock().now();
                auto closureTimeSpan = duration_cast<milliseconds>(t2 - t1);

                if (closureTimeSpan > maxTimeSpanForSvcClosure)
                    maxTimeSpanForSvcClosure = closureTimeSpan;

                return true;
            }

            /// <summary>
            /// Retrieves the maximum closure time for the
            /// web service host registered so far.
            /// </summary>
            /// <return>
            /// Retrieves the maximum closure time in milliseconds.
            /// </return>
            static uint32_t GetMaxClosureTime()
            {
                return static_cast<uint32_t> (maxTimeSpanForSvcClosure.count());
            }

            /// <summary>
            /// Set up the test fixture.
            /// </summary>
            virtual void SetUp() override
            {
                closeServiceRequestEvent.reset(new utils::Event());
            }

            /// <summary>
            /// Tear down the test fixture.
            /// </summary>
            virtual void TearDown() override
            {
                closeServiceRequestEvent.reset();
            }

            /// <summary>
            /// Tests web service access without transport security.
            /// </summary>
            void TestHostTransportUnsecure()
            {
                // Ensures proper initialization/finalization of the framework
                _3fd::core::FrameworkInstance _framework;

                CALL_STACK_TRACE;

                try
                {
                    // Function tables contains the implementations for the operations:
                    CalcBindingUnsecureFunctionTable funcTableSvcUnsecure = {
                        &AddImpl,
                        &MultiplyImpl,
                        &CloseServiceImpl
                    };

                    // Create the web service host with default configurations:
                    SvcEndpointsConfig hostCfg;

                    /* Map the binding used for the unsecure endpoint to
                    the corresponding implementations: */
                    hostCfg.MapBinding(
                        "CalcBindingUnsecure",
                        &calculator_wsdl.contracts.CalcBindingUnsecure,
                        &calculator_wsdl.policies.CalcBindingUnsecure,
                        &funcTableSvcUnsecure,
                        WS_CHANNEL_PROPERTIES{
                            const_cast<WS_CHANNEL_PROPERTY *> (calculator_wsdl.policies.CalcBindingUnsecure.channelProperties),
                            sizeof calculator_wsdl.policies.CalcBindingUnsecure.channelProperties
                        }
                    );
                    
                    // Create the service host:
                    WebServiceHost host(2048);
                    host.Setup("calculator.wsdl", hostCfg, true);
                    host.Open(); // start listening

                    // Wait client to request service closure:
                    ASSERT_TRUE(WaitSignalAndClose(host));
                }
                catch (...)
                {
                    HandleException();
                }
            }

            /// <summary>
            /// Tests web service access with SSL over HTTP
            /// and no client certificate.
            /// </summary>
            void TestHostTransportSslNoClientCert()
            {
                // Ensures proper initialization/finalization of the framework
                _3fd::core::FrameworkInstance _framework;

                CALL_STACK_TRACE;

                try
                {
                    // Function tables contains the implementations for the operations:
                    CalcBindingSSLFunctionTable funcTableSvcSSL = {
                        &AddImpl,
                        &MultiplyImpl,
                        &CloseServiceImpl
                    };

                    // Create the web service host with default configurations:
                    SvcEndpointsConfig hostCfg;

                    /* Map the binding used for the endpoint using SSL over HTTP to
                    the corresponding implementations: */
                    hostCfg.MapBinding(
                        "CalcBindingSSL",
                        &funcTableSvcSSL,
                        CalcBindingSSL_CreateServiceEndpoint
                    );

                    // Create the service host:
                    WebServiceHost host(2048);
                    host.Setup("calculator.wsdl", hostCfg, true);
                    host.Open(); // start listening

                    // Wait client to request service closure:
                    ASSERT_TRUE(WaitSignalAndClose(host));
                }
                catch (...)
                {
                    HandleException();
                }
            }

            /// <summary>
            /// Tests web service access, with SSL over HTTP
            /// and a client certificate.
            /// </summary>
            void TestHostTransportSslWithClientCert()
            {
                // Ensures proper initialization/finalization of the framework
                _3fd::core::FrameworkInstance _framework;

                CALL_STACK_TRACE;

                try
                {
                    // Function tables contains the implementations for the operations:
                    CalcBindingSSLFunctionTable funcTableSvcSSL = {
                        &AddImpl,
                        &MultiplyImpl,
                        &CloseServiceImpl
                    };

                    // Create the web service host with default configurations:
                    SvcEndpointsConfig hostCfg;

                    /* Map the binding used for the endpoint using SSL over HTTP to
                    the corresponding implementations: */
                    hostCfg.MapBinding(
                        "CalcBindingSSL",
                        &funcTableSvcSSL,
                        CalcBindingSSL_CreateServiceEndpoint
                    );

                    // Create the service host:
                    WebServiceHost host(2048);
                    host.Setup("calculator.wsdl", hostCfg, true);
                    host.Open(); // start listening

                    // Wait client to request service closure:
                    ASSERT_TRUE(WaitSignalAndClose(host));
                }
                catch (...)
                {
                    HandleException();
                }
            }

            /// <summary>
            /// Tests SOAP fault transmission by web service.
            /// </summary>
            void TestHostSoapFaultHandling()
            {
                // Ensures proper initialization/finalization of the framework
                _3fd::core::FrameworkInstance _framework;

                CALL_STACK_TRACE;

                try
                {
                    // Function tables contains the implementations for the operations:
                    CalcBindingUnsecureFunctionTable funcTableSvcUnsecure = { Fail, Fail, CloseServiceImpl };
                    CalcBindingSSLFunctionTable funcTableSvcSSL = { Fail, Fail, CloseServiceImpl };

                    // Create the web service host with default configurations:
                    SvcEndpointsConfig hostCfg;

                    /* Map the binding used for the unsecure endpoint to
                    the corresponding implementations: */
                    hostCfg.MapBinding(
                        "CalcBindingUnsecure",
                        &funcTableSvcUnsecure,
                        CalcBindingUnsecure_CreateServiceEndpoint
                    );

                    /* Map the binding used for the endpoint using SSL over HTTP to
                    the corresponding implementations: */
                    hostCfg.MapBinding(
                        "CalcBindingSSL",
                        &funcTableSvcSSL,
                        CalcBindingSSL_CreateServiceEndpoint
                    );

                    // Create the service host:
                    WebServiceHost host(2048);
                    host.Setup("calculator.wsdl", hostCfg, true);
                    host.Open(); // start listening

                    // Wait client to request service closure:
                    ASSERT_TRUE(WaitSignalAndClose(host));
                }
                catch (...)
                {
                    HandleException();
                }
            }
        };

        std::unique_ptr<utils::Event> Framework_WWS_TestCase::closeServiceRequestEvent;

        std::chrono::milliseconds Framework_WWS_TestCase::maxTimeSpanForSvcClosure(0);

		// Implementation for the operation "Add"
		HRESULT CALLBACK AddImpl(
			_In_ const WS_OPERATION_CONTEXT *wsContextHandle,
			_In_ double first,
			_In_ double second,
			_Out_ double *result,
			_In_ const WS_ASYNC_CONTEXT* asyncContext,
			_In_ WS_ERROR *wsErrorHandle)
		{
			*result = first + second;
			return S_OK;
		}

		// Implementation for the operation "Multiply"
		HRESULT CALLBACK MultiplyImpl(
			_In_ const WS_OPERATION_CONTEXT *wsContextHandle,
			_In_ double first,
			_In_ double second,
			_Out_ double *result,
			_In_ const WS_ASYNC_CONTEXT *asyncContext,
			_In_ WS_ERROR *wsErrorHandle)
		{
			*result = first * second;
			return S_OK;
		}

        // Implementation for the operation "CloseService"
        HRESULT CALLBACK CloseServiceImpl(
            _In_ const WS_OPERATION_CONTEXT *wsContextHandle,
            _Out_ __int64 *result,
            _In_ const WS_ASYNC_CONTEXT *asyncContext,
            _In_ WS_ERROR *wsErrorHandle)
        {
            Framework_WWS_TestCase::SignalWebServiceClosureEvent();
            *result = Framework_WWS_TestCase::GetMaxClosureTime();
            return S_OK;
        }

		// Implementation that generates a SOAP fault
		HRESULT CALLBACK Fail(
			_In_ const WS_OPERATION_CONTEXT* wsContextHandle,
			_In_ double first,
			_In_ double second,
			_Out_ double* result,
			_In_ const WS_ASYNC_CONTEXT* asyncContext,
			_In_ WS_ERROR* wsErrorHandle)
		{
			CALL_STACK_TRACE;

			AppException<std::runtime_error> ex(
				"Example of web service fault in operation",
				"Dummy details for fake fault... this message is long on purpose "
				"so as to test code responsible for reading SOAP fault details "
				"in chunks from the buffer... "
				"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vestibulum lacinia, "
				"massa sed efficitur tempor, metus nisl aliquet diam, at lacinia odio est id "
				"risus. Duis porta mi sit amet dui porta, in congue purus finibus. Mauris "
				"feugiat justo id vehicula ullamcorper. Praesent cursus diam id ultrices "
				"scelerisque. Cras tempor neque a augue interdum eleifend. Quisque sed ornare "
				"lorem. Aenean in dictum augue. Duis condimentum maximus sem et suscipit."
			);

			SetSoapFault(ex, "Whatever", wsContextHandle, wsErrorHandle);
			return E_FAIL;
		}

        /// <summary>
        /// Tests synchronous web service access without transport security.
        /// </summary>
		TEST_F(Framework_WWS_TestCase, Host_TransportUnsecure_SyncTest)
		{
            TestHostTransportUnsecure();
		}

		/// <summary>
		/// Tests asynchronous web service access without transport security.
		/// </summary>
		TEST_F(Framework_WWS_TestCase, Host_TransportUnsecure_AsyncTest)
		{
            TestHostTransportUnsecure();
		}

		/// <summary>
		/// Tests synchronous web service access
		/// with SSL over HTTP and no client certificate.
		/// </summary>
		TEST_F(Framework_WWS_TestCase, Host_TransportSSL_NoClientCert_SyncTest)
		{
            TestHostTransportSslNoClientCert();
		}

		/// <summary>
		/// Tests asynchronous web service access with
		/// SSL over HTTP and no client certificate.
		/// </summary>
		TEST_F(Framework_WWS_TestCase, Host_TransportSSL_NoClientCert_AsyncTest)
		{
            TestHostTransportSslNoClientCert();
		}

		/// <summary>
		/// Tests synchronous web service access, with SSL over HTTP
		/// and a client certificate.
		/// </summary>
		TEST_F(Framework_WWS_TestCase, Host_TransportSSL_WithClientCert_SyncTest)
		{
            TestHostTransportSslWithClientCert();
		}

		/// <summary>
		/// Tests asynchronous web service access, with SSL over HTTP
		/// and a client certificate.
		/// </summary>
		TEST_F(Framework_WWS_TestCase, Host_TransportSSL_WithClientCert_AsyncTest)
		{
            TestHostTransportSslWithClientCert();
		}


		/// <summary>
		/// Tests SOAP fault transmission in web service synchronous access.
		/// </summary>
		TEST_F(Framework_WWS_TestCase, Host_SOAP_Fault_SyncTest)
		{
            TestHostSoapFaultHandling();
		}

		/// <summary>
		/// Tests SOAP fault transmission in web service asynchronous access.
		/// </summary>
		TEST_F(Framework_WWS_TestCase, Host_SOAP_Fault_AsyncTest)
		{
            TestHostSoapFaultHandling();
		}

		/// <summary>
		/// Tests web service metadata retrieval via WS-MetadataExchange.
		/// </summary>
		TEST(Framework_WWS_TestCase, DISABLED_Host_MexRequest_TransportUnsecure_Test)
		{
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Function tables contains the implementations for the operations:
				CalcBindingUnsecureFunctionTable funcTableSvcUnsecure = {
					&AddImpl,
					&MultiplyImpl,
                    &CloseServiceImpl
				};

				// Create the web service host with default configurations:
				SvcEndpointsConfig hostCfg;

				/* Map the binding used for the unsecure endpoint to
				the corresponding implementations: */
				hostCfg.MapBinding(
					"CalcBindingUnsecure",
					&funcTableSvcUnsecure,
					CalcBindingUnsecure_CreateServiceEndpoint
				);

				// Create the service host:
				WebServiceHost host(2048);
				host.Setup("calculator.wsdl", hostCfg, true);
				host.Open(); // start listening

				// wait for metadata request...
				std::this_thread::sleep_for(std::chrono::seconds(100));

				host.Close();
			}
			catch (...)
			{
				HandleException();
			}
		}

	}// end of namespace integration_tests
}// end of namespace _3fd
