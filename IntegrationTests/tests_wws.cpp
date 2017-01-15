#include "stdafx.h"
#include "runtime.h"
#include "utils.h"
#include "web_wws_webservicehost.h"
#include "calculator.wsdl.h"

#include <sstream>
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

    HRESULT CALLBACK AuthorizeMessage(
        _In_ const WS_OPERATION_CONTEXT *wsContextHandle,
        _Out_ BOOL *authorized,
        _In_opt_ WS_ERROR *wsErrorHandle)
    {
        CALL_STACK_TRACE;

        HANDLE senderWinToken;
        if (HelpAuthorizeSender(wsContextHandle, &senderWinToken, wsErrorHandle) == STATUS_FAIL)
        {
            *authorized = FALSE;
            return WS_E_SECURITY_VERIFICATION_FAILURE;
        }

        // DO SOMETHING WITH THE WINDOWS TOKEN ...

        *authorized = TRUE;
        /*
        SetSoapFault(
            "Sender is not authorized",
            "Web service host refused to authorize the message sender",
            "AuthorizeSender",
            wsContextHandle,
            wsErrorHandle
        );
        */
        return S_OK;
    }

    /// <summary>
    /// Test fixture for the WWS module.
    /// </summary>
    class Framework_WWS_TestCase : public ::testing::Test
    {
    private:

        static std::unique_ptr<utils::Event> closeServiceRequestEvent;

        static std::chrono::time_point<std::chrono::system_clock> startTimeSvcSetupAndOpen;
        static std::chrono::time_point<std::chrono::system_clock> timeCloseSvcSignalEmission;

        static std::chrono::milliseconds maxTimeSpanForSvcCycle;

    public:

        /// <summary>
        /// Signalize to close the web service host.
        /// </summary>
        static void SignalWebServiceClosureEvent()
        {
            timeCloseSvcSignalEmission = std::chrono::system_clock().now();
            closeServiceRequestEvent->Signalize();
        }

        /// <summary>
        /// Starts the counting time for web service setup and open.
        /// </summary>
        void StartTimeCountWebServiceSetupAndOpen()
        {
            startTimeSvcSetupAndOpen = std::chrono::system_clock().now();
        }

        /// <summary>
        /// Stop counting time for web service setup and open, then wait for
        /// signal to close the web service host.
        /// Once the signal is received, close it and measure how long that takes.
        /// The maximum cycle time (setup, open & close) is kept to respond web clients that
        /// need to know how long to wait before the web service host of the next test is available.
        /// </summary>
        /// <param name="svc">The web service host object.</param>
        /// <returns>
        /// <c>true</c> when the signal was received and the host service
        /// succesfully closed, otherwise, <c>false</c>.
        /// </returns>
        static bool WaitSignalAndClose(WebServiceHost &svc)
        {
            using namespace std::chrono;

            auto stopTimeSvcSetupAndOpen = system_clock().now();

            if (!closeServiceRequestEvent->WaitFor(15000))
                return false;

            /* Wait a little for the client to close its proxy. Otherwise, tests
            have shown that the proxy will fail due to a connection "abnormally
            terminated". */
            std::this_thread::sleep_for(std::chrono::milliseconds(8));

            if (!svc.Close())
                return false;

            auto closureTimeSpan = duration_cast<milliseconds>(
                system_clock().now() - timeCloseSvcSignalEmission
            );

            auto setupAndOpenTimeSpan = duration_cast<milliseconds>(
                stopTimeSvcSetupAndOpen - startTimeSvcSetupAndOpen
            );

            auto cycleTimeSpan = setupAndOpenTimeSpan + closureTimeSpan;

            if (cycleTimeSpan > maxTimeSpanForSvcCycle)
                maxTimeSpanForSvcCycle = cycleTimeSpan;

            std::ostringstream oss;
            oss << "Max registered time span for web service host cycle close-setup-open is "
                << maxTimeSpanForSvcCycle.count() << " ms";

            Logger::Write(oss.str(), Logger::PRIO_NOTICE);

            return true;
        }

        /// <summary>
        /// Retrieves the amount of time expected to last
        /// a full cycle in server of close-setup-open, based
        /// on the maximum closure time for the web service
        /// host registered so far.
        /// </summary>
        /// <return>
        /// Retrieves the maximum closure time in milliseconds.
        /// </return>
        static uint32_t GetEstimateCycleTime()
        {
            /* In practice, measured time must be linearly
            augmented for adjustment (using field data), because
            apparently the server takes much longer to be available
            than what the measures point out. It happens a little
            after WebServiceHost::Open returns... */
            return static_cast<uint32_t> (maxTimeSpanForSvcCycle.count() + 50);
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
                CalcBindingUnsecureFunctionTable funcTableSvc = {
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
                    &funcTableSvc
                );
                
                StartTimeCountWebServiceSetupAndOpen();

                // Create the service host:
                WebServiceHost host(2048);
                host.Setup("calculator.wsdl", hostCfg, nullptr, true);
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
        /// Tests web service access with SSL over HTTP.
        /// </summary>
        void TestHostTransportSSL()
        {
            // Ensures proper initialization/finalization of the framework
            _3fd::core::FrameworkInstance _framework;

            CALL_STACK_TRACE;

            try
            {
                // Function tables contains the implementations for the operations:
                CalcBindingSSLFunctionTable funcTableSvc = {
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
                    &calculator_wsdl.contracts.CalcBindingSSL,
                    &calculator_wsdl.policies.CalcBindingSSL,
                    &funcTableSvc
                );

                StartTimeCountWebServiceSetupAndOpen();

                // Create the service host:
                WebServiceHost host(2048);
                host.Setup("calculator.wsdl", hostCfg, nullptr, true);
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
        /// Tests web service access with HTTP header authorization and SSL.
        /// </summary>
        void TestHostHttpHeaderAuthTransportSSL()
        {
            // Ensures proper initialization/finalization of the framework
            _3fd::core::FrameworkInstance _framework;

            CALL_STACK_TRACE;

            try
            {
                // Function tables contains the implementations for the operations:
                CalcBindingHeaderAuthSSLFunctionTable funcTableSvc = {
                    &AddImpl,
                    &MultiplyImpl,
                    &CloseServiceImpl
                };

                // Create the web service host with default configurations:
                SvcEndpointsConfig hostCfg;

                /* Map the binding used for the endpoint using HTTP header
                authorization with SSL the corresponding implementations: */
                hostCfg.MapBinding(
                    "CalcBindingHeaderAuthSSL",
                    &calculator_wsdl.contracts.CalcBindingHeaderAuthSSL,
                    &calculator_wsdl.policies.CalcBindingHeaderAuthSSL,
                    &funcTableSvc
                );

                StartTimeCountWebServiceSetupAndOpen();

                // Create the service host:
                WebServiceHost host(2048);
                host.Setup("calculator.wsdl", hostCfg, &AuthorizeMessage, true);
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
                CalcBindingHeaderAuthSSLFunctionTable funcTableSvcHeaderAuthSSL = { Fail, Fail, CloseServiceImpl };

                // Create the web service host with default configurations:
                SvcEndpointsConfig hostCfg;

                /* Map the binding used for the unsecure endpoint
                to the corresponding implementations: */
                hostCfg.MapBinding(
                    "CalcBindingUnsecure",
                    &calculator_wsdl.contracts.CalcBindingUnsecure,
                    &calculator_wsdl.policies.CalcBindingUnsecure,
                    &funcTableSvcUnsecure
                );

                /* Map the binding used for the endpoint using SSL
                over HTTP to the corresponding implementations: */
                hostCfg.MapBinding(
                    "CalcBindingSSL",
                    &calculator_wsdl.contracts.CalcBindingSSL,
                    &calculator_wsdl.policies.CalcBindingSSL,
                    &funcTableSvcSSL
                );

                /* Map the binding used for the endpoint using HTTP header
                authorization with SSL the corresponding implementations: */
                hostCfg.MapBinding(
                    "CalcBindingHeaderAuthSSL",
                    &calculator_wsdl.contracts.CalcBindingHeaderAuthSSL,
                    &calculator_wsdl.policies.CalcBindingHeaderAuthSSL,
                    &funcTableSvcHeaderAuthSSL
                );

                StartTimeCountWebServiceSetupAndOpen();

                // Create the service host:
                WebServiceHost host(3072);
                host.Setup("calculator.wsdl", hostCfg, nullptr, true);
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

    std::chrono::time_point<std::chrono::system_clock> Framework_WWS_TestCase::startTimeSvcSetupAndOpen;
    std::chrono::time_point<std::chrono::system_clock> Framework_WWS_TestCase::timeCloseSvcSignalEmission;

    std::chrono::milliseconds Framework_WWS_TestCase::maxTimeSpanForSvcCycle(0);

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
        *result = Framework_WWS_TestCase::GetEstimateCycleTime();
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
        TestHostTransportSSL();
	}

	/// <summary>
	/// Tests asynchronous web service access with
	/// SSL over HTTP and no client certificate.
	/// </summary>
	TEST_F(Framework_WWS_TestCase, Host_TransportSSL_NoClientCert_AsyncTest)
	{
        TestHostTransportSSL();
	}

	/// <summary>
	/// Tests synchronous web service access, with SSL over HTTP
	/// and a client certificate.
	/// </summary>
	TEST_F(Framework_WWS_TestCase, Host_TransportSSL_WithClientCert_SyncTest)
	{
        TestHostTransportSSL();
	}

	/// <summary>
	/// Tests asynchronous web service access, with SSL over HTTP
	/// and a client certificate.
	/// </summary>
	TEST_F(Framework_WWS_TestCase, Host_TransportSSL_WithClientCert_AsyncTest)
	{
        TestHostTransportSSL();
	}

    /// <summary>
    /// Tests synchronous web service access, with HTTP header
    /// authorization, SSL and a client certificate.
    /// </summary>
    TEST_F(Framework_WWS_TestCase, Host_HeaderAuthTransportSSL_WithClientCert_SyncTest)
    {
        TestHostHttpHeaderAuthTransportSSL();
    }

    /// <summary>
    /// Tests asynchronous web service access, with HTTP header
    /// authorization, SSL and a client certificate.
    /// </summary>
    TEST_F(Framework_WWS_TestCase, Host_HeaderAuthTransportSSL_WithClientCert_AsyncTest)
    {
        TestHostHttpHeaderAuthTransportSSL();
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
                &calculator_wsdl.contracts.CalcBindingUnsecure,
                &calculator_wsdl.policies.CalcBindingUnsecure,
                &funcTableSvcUnsecure
			);

			// Create the service host:
			WebServiceHost host(2048);
			host.Setup("calculator.wsdl", hostCfg, nullptr, true);
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
