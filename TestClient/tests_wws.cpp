#include "stdafx.h"
#include "runtime.h"
#include "web_wws_webservicehost.h"
#include "web_wws_webserviceproxy.h"
#include "calculator.wsdl.h"

#include <vector>

namespace _3fd
{
	namespace integration_tests
	{
		using namespace core;
		using namespace web::wws;

		void HandleException();

		const size_t proxyOperHeapSize(4096);

		/////////////////////////////////////
		// Proxy without transport security
		/////////////////////////////////////

		/// <summary>
		/// Implements a client for the calculator web service without transport security.
		/// </summary>
		class CalcSvcProxyUnsecure : public WebServiceProxy
		{
		public:

			CalcSvcProxyUnsecure(const SvcProxyConfig &config) :
				WebServiceProxy(
					"http://tars:81/calculator",
					config,
					&CreateWSProxy<WS_HTTP_BINDING_TEMPLATE, CalcBindingUnsecure_CreateServiceProxy>
				)
			{}

			// Synchronous 'Add' operation
			double Add(double first, double second)
			{
				CALL_STACK_TRACE;

				double result;
				WSHeap heap(proxyOperHeapSize);
				WSError err;
				HRESULT hr =
					CalcBindingUnsecure_Add(
						GetHandle(),
						first,
						second,
						&result,
						heap.GetHandle(),
						nullptr, 0,
						nullptr,
						err.GetHandle()
					);

				err.RaiseExClientNotOK(hr, "Calculator web service returned an error", heap);

				return result;
			}

			// Synchronous 'Multiply' operation
			double Multiply(double first, double second)
			{
				CALL_STACK_TRACE;

				double result;
				WSHeap heap(proxyOperHeapSize);
				WSError err;
				HRESULT hr =
					CalcBindingUnsecure_Multiply(
						GetHandle(),
						first,
						second,
						&result,
						heap.GetHandle(),
						nullptr, 0,
						nullptr,
						err.GetHandle()
					);

				err.RaiseExClientNotOK(hr, "Calculator web service returned an error", heap);

				return result;
			}

			// Asynchronous 'Multiply' operation
			WSAsyncOper MultiplyAsync(double first, double second, double &result)
			{
				CALL_STACK_TRACE;

				// Prepare for an asynchronous operation:
				auto asyncOp = CreateAsyncOperation(proxyOperHeapSize);
				auto asyncContext = asyncOp.GetContext();
				
				HRESULT hr = // this is immediately returned HRESULT code
					CalcBindingUnsecure_Multiply(
						GetHandle(),
						first,
						second,
						&result,
						asyncOp.GetHeapHandle(),
						nullptr, 0,
						&asyncContext, // this parameter asks for an asynchronous call
						asyncOp.GetErrHelperHandle()
					);

				asyncOp.SetCallReturn(hr);
				return std::move(asyncOp);
			}

            // 'CloseService' operation
            uint32_t CloseHostService()
            {
                CALL_STACK_TRACE;

                int64_t result;
                WSHeap heap(proxyOperHeapSize);
                WSError err;
                HRESULT hr =
                    CalcBindingUnsecure_CloseService(
                        GetHandle(),
                        &result,
                        heap.GetHandle(),
                        nullptr, 0,
                        nullptr,
                        err.GetHandle()
                    );

                err.RaiseExClientNotOK(hr, "Calculator web service returned an error", heap);

                return static_cast<uint32_t> (result);
            }
		};

		/// <summary>
		/// Tests synchronous web service access without transport security.
		/// </summary>
		TEST(Framework_WWS_TestCase, Proxy_TransportUnsecure_SyncTest)
		{
            system("pause");

			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create the proxy (client):
				SvcProxyConfig proxyCfg;
				CalcSvcProxyUnsecure client(proxyCfg);
				client.Open();

				for (int count = 0; count < 10; ++count)
				{
					EXPECT_EQ(666.0, client.Add(606.0, 60.0));
					EXPECT_EQ(666.0, client.Multiply(111.0, 6.0));
				}

                /* Request the host to close the service. The host will respond
                with the max expected duration (in ms) for that to complete: */
                auto timeout = client.CloseHostService();
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
				client.Close();
			}
			catch (...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests asynchronous web service access without transport security.
		/// </summary>
		TEST(Framework_WWS_TestCase, Proxy_TransportUnsecure_AsyncTest)
		{
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create the proxy (client):
				SvcProxyConfig proxyCfg;
				CalcSvcProxyUnsecure client(proxyCfg);
				client.Open();

				const int maxAsyncCalls = 5;

				std::vector<double> results(maxAsyncCalls);
				std::vector<WSAsyncOper> asyncOps;
				asyncOps.reserve(maxAsyncCalls);

				// Fire the asynchronous requests:
				for (int idx = 0; idx < maxAsyncCalls; ++idx)
				{
					asyncOps.push_back(
						client.MultiplyAsync(111.0, 6.0, results[idx])
					);
				}

				// Get the results and check for errors:
				while (!asyncOps.empty())
				{
					asyncOps.back()
						.RaiseExClientNotOK("Calculator web service returned an error");

					asyncOps.pop_back();

					EXPECT_EQ(666.0, results.back());
					results.pop_back();
				}

                /* Request the host to close the service. The host will respond
                with the max expected duration (in ms) for that to complete: */
                auto timeout = client.CloseHostService();
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                client.Close();
			}
			catch (...)
			{
				HandleException();
			}
		}

		////////////////////////////////
		// Proxy with SSL over HTTP
		////////////////////////////////

		/// <summary>
		/// Implements a client for the calculator web service with SSL security.
		/// </summary>
		class CalcSvcProxySSL : public WebServiceProxy
		{
		public:

			// Ctor for proxy without client certificate
			CalcSvcProxySSL(const SvcProxyConfig &config) :
				WebServiceProxy(
					"https://localhost:8989/calculator",
					config,
					&CreateWSProxy<WS_HTTP_SSL_BINDING_TEMPLATE, CalcBindingSSL_CreateServiceProxy>
				)
			{}

			// Ctor for proxy using a client certificate
			CalcSvcProxySSL(const SvcProxyConfig &config, const SvcProxyCertInfo &certInfo) :
				WebServiceProxy(
					"https://localhost:8989/calculator",
					config,
					certInfo,
					CalcBindingSSL_CreateServiceProxy
				)
			{}

			double Add(double first, double second)
			{
				CALL_STACK_TRACE;

				double result;
				WSHeap heap(proxyOperHeapSize);
				WSError err;
				HRESULT hr =
					CalcBindingSSL_Add(
						GetHandle(),
						first,
						second,
						&result,
						heap.GetHandle(),
						nullptr, 0,
						nullptr,
						err.GetHandle()
					);

				err.RaiseExClientNotOK(hr, "Calculator web service returned an error", heap);

				return result;
			}

			double Multiply(double first, double second)
			{
				CALL_STACK_TRACE;

				double result;
				WSHeap heap(proxyOperHeapSize);
				WSError err;
				HRESULT hr =
					CalcBindingSSL_Multiply(
						GetHandle(),
						first,
						second,
						&result,
						heap.GetHandle(),
						nullptr, 0,
						nullptr,
						err.GetHandle()
					);

				err.RaiseExClientNotOK(hr, "Calculator web service returned an error", heap);

				return result;
			}

			// Asynchronous 'Multiply' operation
			WSAsyncOper MultiplyAsync(double first, double second, double &result)
			{
				CALL_STACK_TRACE;

				// Prepare for an asynchronous operation:
				auto asyncOp = CreateAsyncOperation(proxyOperHeapSize);
				auto asyncContext = asyncOp.GetContext();

				HRESULT hr = // this is immediately returned HRESULT code
					CalcBindingSSL_Multiply(
						GetHandle(),
						first,
						second,
						&result,
						asyncOp.GetHeapHandle(),
						nullptr, 0,
						&asyncContext, // this parameter asks for an asynchronous call
						asyncOp.GetErrHelperHandle()
					);

				asyncOp.SetCallReturn(hr);
				return std::move(asyncOp);
			}

            // 'CloseService' operation
            uint32_t CloseHostService()
            {
                CALL_STACK_TRACE;

                int64_t result;
                WSHeap heap(proxyOperHeapSize);
                WSError err;
                HRESULT hr =
                    CalcBindingSSL_CloseService(
                        GetHandle(),
                        &result,
                        heap.GetHandle(),
                        nullptr, 0,
                        nullptr,
                        err.GetHandle()
                    );

                err.RaiseExClientNotOK(hr, "Calculator web service returned an error", heap);

                return result;
            }
		};

		// Thumbprint of client side certificate for transport security
		const char *clientCertificateThumbprint = "fa6040bc28b9b50ec77c2f40b94125c2f775087f";

		/// <summary>
		/// Tests synchronous web service access
		/// with SSL over HTTP and no client certificate.
		/// </summary>
		TEST(Framework_WWS_TestCase, Proxy_TransportSSL_NoClientCert_SyncTest)
		{
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create the proxy (client):
				SvcProxyConfig proxyCfg;
				CalcSvcProxySSL client(proxyCfg);
				client.Open();

				for (int count = 0; count < 10; ++count)
				{
					EXPECT_EQ(666.0, client.Add(606.0, 60.0));
					EXPECT_EQ(666.0, client.Multiply(111.0, 6.0));
				}

                /* Request the host to close the service. The host will respond
                with the max expected duration (in ms) for that to complete: */
                auto timeout = client.CloseHostService();
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                client.Close();
			}
			catch (...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests asynchronous web service access with
		/// SSL over HTTP and no client certificate.
		/// </summary>
		TEST(Framework_WWS_TestCase, Proxy_TransportSSL_NoClientCert_AsyncTest)
		{
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create the proxy (client):
				SvcProxyConfig proxyCfg;
				CalcSvcProxySSL client(proxyCfg);
				client.Open();

				const int maxAsyncCalls = 5;

				std::vector<double> results(maxAsyncCalls);
				std::vector<WSAsyncOper> asyncOps;
				asyncOps.reserve(maxAsyncCalls);

				// Fire the asynchronous requests:
				for (int idx = 0; idx < maxAsyncCalls; ++idx)
				{
					asyncOps.push_back(
						client.MultiplyAsync(111.0, 6.0, results[idx])
					);
				}

				// Get the results and check for errors:
				while (!asyncOps.empty())
				{
					asyncOps.back()
						.RaiseExClientNotOK("Calculator web service returned an error");

					asyncOps.pop_back();

					EXPECT_EQ(666.0, results.back());
					results.pop_back();
				}

                /* Request the host to close the service. The host will respond
                with the max expected duration (in ms) for that to complete: */
                auto timeout = client.CloseHostService();
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                client.Close();
			}
			catch (...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests synchronous web service access, with SSL over HTTP
		/// and and a client certificate.
		/// </summary>
		TEST(Framework_WWS_TestCase, Proxy_TransportSSL_WithClientCert_SyncTest)
		{
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				/* Insert here the information describing the client side
				certificate to use in your test environment: */
				SvcProxyCertInfo proxyCertInfo(
					CERT_SYSTEM_STORE_LOCAL_MACHINE,
					"My",
					clientCertificateThumbprint
				);

				// Create the proxy (client):
				SvcProxyConfig proxyCfg;
				CalcSvcProxySSL client(proxyCfg, proxyCertInfo);
				client.Open();

				for (int count = 0; count < 10; ++count)
				{
					EXPECT_EQ(666.0, client.Add(606.0, 60.0));
					EXPECT_EQ(666.0, client.Multiply(111.0, 6.0));
				}

                /* Request the host to close the service. The host will respond
                with the max expected duration (in ms) for that to complete: */
                auto timeout = client.CloseHostService();
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                client.Close();
			}
			catch (...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests asynchronous web service access, with SSL over HTTP
		/// and and a client certificate.
		/// </summary>
		TEST(Framework_WWS_TestCase, Proxy_TransportSSL_WithClientCert_AsyncTest)
		{
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				/* Insert here the information describing the client side
				certificate to use in your test environment: */
				SvcProxyCertInfo proxyCertInfo(
					CERT_SYSTEM_STORE_LOCAL_MACHINE,
					"My",
					clientCertificateThumbprint
				);

				// Create the proxy (client):
				SvcProxyConfig proxyCfg;
				CalcSvcProxySSL client(proxyCfg, proxyCertInfo);
				client.Open();

				const int maxAsyncCalls = 5;

				std::vector<double> results(maxAsyncCalls);
				std::vector<WSAsyncOper> asyncOps;
				asyncOps.reserve(maxAsyncCalls);

				// Fire the asynchronous requests:
				for (int idx = 0; idx < maxAsyncCalls; ++idx)
				{
					asyncOps.push_back(
						client.MultiplyAsync(111.0, 6.0, results[idx])
					);
				}

				// Get the results and check for errors:
				while (!asyncOps.empty())
				{
					asyncOps.back()
						.RaiseExClientNotOK("Calculator web service returned an error");

					asyncOps.pop_back();

					EXPECT_EQ(666.0, results.back());
					results.pop_back();
				}

                /* Request the host to close the service. The host will respond
                with the max expected duration (in ms) for that to complete: */
                auto timeout = client.CloseHostService();
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                client.Close();
			}
			catch (...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests SOAP fault transmission in web service synchronous access.
		/// </summary>
		TEST(Framework_WWS_TestCase, Proxy_SoapFault_SyncTest)
		{
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				SvcProxyConfig proxyCfg; // proxy configuration with default values

                // Create a proxy without transport security:
                CalcSvcProxyUnsecure unsecureClient(proxyCfg);
                unsecureClient.Open();

				try
				{
					// This request should generate a SOAP fault, hence throwing an exception
					unsecureClient.Add(606.0, 60.0);
				}
				catch (IAppException &ex)
				{// Log the fault:
					Logger::Write(ex, Logger::PRIO_ERROR);
				}

                unsecureClient.Close();

				/* Insert here the information describing the client side
				certificate to use in your test environment: */
				SvcProxyCertInfo proxyCertInfo(
					CERT_SYSTEM_STORE_LOCAL_MACHINE,
					"My",
					clientCertificateThumbprint
				);

                // Create a secure proxy (client):
                CalcSvcProxySSL sslClient(proxyCfg, proxyCertInfo);
                sslClient.Open();

				try
				{
					// This request should generate a SOAP fault, hence throwing an exception
					sslClient.Multiply(111.0, 6.0);
				}
				catch (IAppException &ex)
				{// Log the fault:
					Logger::Write(ex, Logger::PRIO_ERROR);
				}

                /* Request the host to close the service. The host will respond
                with the max expected duration (in ms) for that to complete: */
                auto timeout = sslClient.CloseHostService();
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                sslClient.Close();
			}
			catch (...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests SOAP fault transmission in web service asynchronous access.
		/// </summary>
		TEST(Framework_WWS_TestCase, Proxy_SoapFault_AsyncTest)
		{
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				SvcProxyConfig proxyCfg; // proxy configuration with default values

                // Create a proxy without transport security:
                CalcSvcProxyUnsecure unsecureClient(proxyCfg);
                unsecureClient.Open();

				try
				{
					// Start an asynchronous request:
					double result;
					auto asyncOp = unsecureClient.MultiplyAsync(606.0, 60.0, result);

					/* The request generates a SOAP fault. This will wait for its
					completion, then it throws an exception made from the deserialized
					SOAP fault response: */
					asyncOp.RaiseExClientNotOK("Calculator web service returned an error");
				}
				catch (IAppException &ex)
				{// Log the fault:
					Logger::Write(ex, Logger::PRIO_ERROR);
				}

                unsecureClient.Close();

				/* Insert here the information describing the client side
				certificate to use in your test environment: */
				SvcProxyCertInfo proxyCertInfo(
					CERT_SYSTEM_STORE_LOCAL_MACHINE,
					"My",
					clientCertificateThumbprint
				);

                // Create a secure proxy (client):
                CalcSvcProxySSL sslClient(proxyCfg, proxyCertInfo);
                sslClient.Open();

				try
				{
					// This request should generate a SOAP fault, hence throwing an exception
					double result;
					auto asyncOp = sslClient.MultiplyAsync(111.0, 6.0, result);

					/* The request generates a SOAP fault. This will wait for its
					completion, then it throws an exception made from the deserialized
					SOAP fault response: */
					asyncOp.RaiseExClientNotOK("Calculator web service returned an error");
				}
				catch (IAppException &ex)
				{// Log the fault:
					Logger::Write(ex, Logger::PRIO_ERROR);
				}

                /* Request the host to close the service. The host will respond
                with the max expected duration (in ms) for that to complete: */
                auto timeout = sslClient.CloseHostService();
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                sslClient.Close();
			}
			catch (...)
			{
				HandleException();
			}
		}

	}// end of namespace integration_tests
}// end of namespace _3fd
