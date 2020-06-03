#include "pch.h"
#include <3fd/core/runtime.h>
#include <3fd/core/configuration.h>
#include <3fd/wws/web_wws_webserviceproxy.h>

#include "calculator.wsdl.h"

#include <chrono>
#include <thread>
#include <vector>

#define UNDEF_HOST_UNSEC     "WEB SERVICE HOST UNSECURE ENDPOINT IS NOT DEFINED"
#define UNDEF_HOST_SSL       "WEB SERVICE HOST SSL ENDPOINT IS NOT DEFINED"
#define UNDEF_CLIENT_CERT    "WEB SERVICE CLIENT SIDE CERTIFICATE THUMBPRINT IS UNDEFINED"
#define UNDEF_HOST_SSL_HAUTH "WEB SERVICE HOST SSL WITH HEADER AUTHORIZATION ENDPOINT IS NOT DEFINED"

namespace _3fd
{
namespace integration_tests
{
    using namespace core;
    using namespace web::wws;

    void HandleException();

    const size_t proxyOperHeapSize(4096);

    /// <summary>
    /// Stalls the client application a little before firing requests.
    /// </summary>
    /// <remarks>   
    /// This client application switches from one test to another faster than the server side,
    /// so we need to stall a little.Otherwise this test might end up being serviced by the endpoint
    /// of the previous test which is still open.The consequence is that when the right endpoint come
    /// online late, it never receives any request, never closes, times out and fails in the server side.
    /// </remarks>
    static void Stall()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(32));
    }

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
                AppConfig::GetSettings().application.GetString("testWwsHostUnsecEndpoint", UNDEF_HOST_UNSEC),
                config,
                &CreateWSProxy<WS_HTTP_BINDING_TEMPLATE, CalcBindingUnsecure_CreateServiceProxy>
            )
        {}

        // Synchronous 'Add' operation
        double Add(double first, double second)
        {
            CALL_STACK_TRACE;

            double result;

            Call("Calculator web service operation 'Add'",
                proxyOperHeapSize,
                [=,&result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                {
                    return CalcBindingUnsecure_Add(wsProxyHandle,
                                                   first,
                                                   second,
                                                   &result,
                                                   wsHeapHandle,
                                                   nullptr, 0,
                                                   nullptr,
                                                   wsErrorHandle);
                });

            return result;
        }

        // Synchronous 'Multiply' operation
        double Multiply(double first, double second)
        {
            CALL_STACK_TRACE;

            double result;

            Call("Calculator web service operation 'Multiply'",
                 proxyOperHeapSize,
                 [=, &result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                 {
                     return CalcBindingUnsecure_Multiply(wsProxyHandle,
                                                         first,
                                                         second,
                                                         &result,
                                                         wsHeapHandle,
                                                         nullptr, 0,
                                                         nullptr,
                                                         wsErrorHandle);
                 });

            return result;
        }

        // Asynchronous 'Multiply' operation
        std::future<void> MultiplyAsync(double first, double second, double &result)
        {
            CALL_STACK_TRACE;

            return CallAsync("Calculator web service operation 'Multiply'",
                             proxyOperHeapSize,
                             [=, &result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                             {
                                 return CalcBindingUnsecure_Multiply(wsProxyHandle,
                                                                     first,
                                                                     second,
                                                                     &result,
                                                                     wsHeapHandle,
                                                                     nullptr, 0,
                                                                     nullptr,
                                                                     wsErrorHandle);
                             });
        }

        // 'CloseService' operation
        bool CloseHostService()
        {
            CALL_STACK_TRACE;

            BOOL result(FALSE);

            Call("Calculator web service operation 'CloseService'",
                 proxyOperHeapSize,
                 [&result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                 {
                     return CalcBindingUnsecure_CloseService(wsProxyHandle,
                                                             &result,
                                                             wsHeapHandle,
                                                             nullptr, 0,
                                                             nullptr,
                                                             wsErrorHandle);
                 });

            return (result == TRUE);
        }
    };


    /// <summary>
    /// Tests synchronous web service access without transport security.
    /// </summary>
    TEST(Framework_WWS_TestCase, Proxy_TransportUnsecure_SyncTest)
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

            for (int count = 0; count < 10; ++count)
            {
                EXPECT_EQ(666.0, client.Add(606.0, 60.0));
                EXPECT_EQ(666.0, client.Multiply(111.0, 6.0));
            }

            EXPECT_TRUE(client.CloseHostService());
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
            Stall();

            // Create the proxy (client):
            SvcProxyConfig proxyCfg;
            CalcSvcProxyUnsecure client(proxyCfg);
            client.Open();

            const int maxAsyncCalls = 5;

            std::vector<double> results(maxAsyncCalls);
            std::vector<std::future<void>> asyncOps;
            asyncOps.reserve(maxAsyncCalls);

            // Fire the asynchronous requests:
            for (int idx = 0; idx < maxAsyncCalls; ++idx)
            {
                asyncOps.push_back(
                    client.MultiplyAsync(111.0, 6.0, results[idx])
                );
            }

            // Get the results and check for errors:
            for (int idx = 0; idx < maxAsyncCalls; ++idx)
            {
                asyncOps[idx].get();
                EXPECT_EQ(666.0, results[idx]);
            }

            EXPECT_TRUE(client.CloseHostService());
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
                AppConfig::GetSettings().application.GetString("testWwsHostSslEndpoint", UNDEF_HOST_SSL),
                config,
                &CreateWSProxy<WS_HTTP_SSL_BINDING_TEMPLATE, CalcBindingSSL_CreateServiceProxy>
            )
        {}

        // Ctor for proxy using a client certificate
        CalcSvcProxySSL(const SvcProxyConfig &config, const SvcProxyCertInfo &certInfo) :
            WebServiceProxy(
                AppConfig::GetSettings().application.GetString("testWwsHostSslEndpoint", UNDEF_HOST_SSL),
                config,
                certInfo,
                CalcBindingSSL_CreateServiceProxy
            )
        {}

        // Synchronous 'Add' operation
        double Add(double first, double second)
        {
            CALL_STACK_TRACE;

            double result;

            Call("Calculator web service operation 'Add'",
                 proxyOperHeapSize,
                 [=, &result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                 {
                     return CalcBindingSSL_Add(wsProxyHandle,
                                               first,
                                               second,
                                               &result,
                                               wsHeapHandle,
                                               nullptr, 0,
                                               nullptr,
                                               wsErrorHandle);
                 });

            return result;
        }

        // Synchronous 'Multiply' operation
        double Multiply(double first, double second)
        {
            CALL_STACK_TRACE;

            double result;

            Call("Calculator web service operation 'Multiply'",
                 proxyOperHeapSize,
                 [=, &result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                 {
                     return CalcBindingSSL_Multiply(wsProxyHandle,
                                                    first,
                                                    second,
                                                    &result,
                                                    wsHeapHandle,
                                                    nullptr, 0,
                                                    nullptr,
                                                    wsErrorHandle);
                 });

            return result;
        }

        // Asynchronous 'Multiply' operation
        std::future<void> MultiplyAsync(double first, double second, double &result)
        {
            CALL_STACK_TRACE;

            return CallAsync("Calculator web service operation 'Multiply'",
                             proxyOperHeapSize,
                             [=, &result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                             {
                                 return CalcBindingSSL_Multiply(wsProxyHandle,
                                                                first,
                                                                second,
                                                                &result,
                                                                wsHeapHandle,
                                                                nullptr, 0,
                                                                nullptr,
                                                                wsErrorHandle);
                             });
        }

        // Synchronous 'CloseService' operation
        bool CloseHostService()
        {
            CALL_STACK_TRACE;

            BOOL result;

            Call("Calculator web service operation 'CloseService'",
                 proxyOperHeapSize,
                 [&result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                 {
                     return CalcBindingSSL_CloseService(wsProxyHandle,
                                                        &result,
                                                        wsHeapHandle,
                                                        nullptr, 0,
                                                        nullptr,
                                                        wsErrorHandle);
                 });

            return (result == TRUE);
        }
    };

    // Thumbprint of client side certificate for transport security
    const char *keyForCliCertThumbprint("testWwsCliCertThumbprint");

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

            EXPECT_TRUE(client.CloseHostService());
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
            Stall();

            // Create the proxy (client):
            SvcProxyConfig proxyCfg;
            CalcSvcProxySSL client(proxyCfg);
            client.Open();

            const int maxAsyncCalls = 5;

            std::vector<double> results(maxAsyncCalls);
            std::vector<std::future<void>> asyncOps;
            asyncOps.reserve(maxAsyncCalls);

            // Fire the asynchronous requests:
            for (int idx = 0; idx < maxAsyncCalls; ++idx)
            {
                asyncOps.push_back(
                    client.MultiplyAsync(111.0, 6.0, results[idx])
                );
            }

            // Get the results and check for errors:
            for (int idx = 0; idx < maxAsyncCalls; ++idx)
            {
                asyncOps[idx].get();
                EXPECT_EQ(666.0, results[idx]);
            }

            EXPECT_TRUE(client.CloseHostService());
            client.Close();
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests synchronous web service access, with
    /// SSL over HTTP and a client certificate.
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
                AppConfig::GetSettings().application.GetString(keyForCliCertThumbprint, UNDEF_CLIENT_CERT)
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

            EXPECT_TRUE(client.CloseHostService());
            client.Close();
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests asynchronous web service access, with
    /// SSL over HTTP and a client certificate.
    /// </summary>
    TEST(Framework_WWS_TestCase, Proxy_TransportSSL_WithClientCert_AsyncTest)
    {
        // Ensures proper initialization/finalization of the framework
        _3fd::core::FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            Stall();

            /* Insert here the information describing the client side
            certificate to use in your test environment: */
            SvcProxyCertInfo proxyCertInfo(
                CERT_SYSTEM_STORE_LOCAL_MACHINE,
                "My",
                AppConfig::GetSettings().application.GetString(keyForCliCertThumbprint, UNDEF_CLIENT_CERT)
            );

            // Create the proxy (client):
            SvcProxyConfig proxyCfg;
            CalcSvcProxySSL client(proxyCfg, proxyCertInfo);
            client.Open();

            const int maxAsyncCalls = 5;

            std::vector<double> results(maxAsyncCalls);
            std::vector<std::future<void>> asyncOps;
            asyncOps.reserve(maxAsyncCalls);

            // Fire the asynchronous requests:
            for (int idx = 0; idx < maxAsyncCalls; ++idx)
            {
                asyncOps.push_back(
                    client.MultiplyAsync(111.0, 6.0, results[idx])
                );
            }

            // Get the results and check for errors:
            for (int idx = 0; idx < maxAsyncCalls; ++idx)
            {
                asyncOps[idx].get();
                EXPECT_EQ(666.0, results[idx]);
            }

            EXPECT_TRUE(client.CloseHostService());
            client.Close();
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Implements a client for the calculator web service with SSL security.
    /// </summary>
    class CalcSvcProxyHeaderAuthSSL : public WebServiceProxy
    {
    public:

        // Ctor for proxy using a client certificate
        CalcSvcProxyHeaderAuthSSL(const SvcProxyConfig &config, const SvcProxyCertInfo &certInfo) :
            WebServiceProxy(
                AppConfig::GetSettings().application.GetString("testWwsHostHAuthEndpoint", UNDEF_HOST_SSL_HAUTH),
                config,
                certInfo,
                CalcBindingHeaderAuthSSL_CreateServiceProxy
            )
        {}

        // Synchronous 'Add' operation
        double Add(double first, double second)
        {
            CALL_STACK_TRACE;

            double result;

            Call("Calculator web service operation 'Add'",
                 proxyOperHeapSize,
                 [=, &result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                 {
                     return CalcBindingHeaderAuthSSL_Add(wsProxyHandle,
                                                         first,
                                                         second,
                                                         &result,
                                                         wsHeapHandle,
                                                         nullptr, 0,
                                                         nullptr,
                                                         wsErrorHandle);
                 });

            return result;
        }

        // Synchronous 'Multiply' operation
        double Multiply(double first, double second)
        {
            CALL_STACK_TRACE;

            double result;

            Call("Calculator web service operation 'Multiply'",
                 proxyOperHeapSize,
                 [=, &result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                 {
                     return CalcBindingHeaderAuthSSL_Multiply(wsProxyHandle,
                                                              first,
                                                              second,
                                                              &result,
                                                              wsHeapHandle,
                                                              nullptr, 0,
                                                              nullptr,
                                                              wsErrorHandle);
                 });

            return result;
        }

        // Asynchronous 'Multiply' operation
        std::future<void> MultiplyAsync(double first, double second, double &result)
        {
            CALL_STACK_TRACE;

            return CallAsync(
                "Calculator web service operation 'Multiply'",
                proxyOperHeapSize,
                [=, &result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                {
                    return CalcBindingHeaderAuthSSL_Multiply(wsProxyHandle,
                                                             first,
                                                             second,
                                                             &result,
                                                             wsHeapHandle,
                                                             nullptr, 0,
                                                             nullptr,
                                                             wsErrorHandle);
                }
            );
        }

        // 'CloseService' operation
        bool CloseHostService()
        {
            CALL_STACK_TRACE;

            BOOL result;
            
            Call("Calculator web service operation 'CloseService'",
                 proxyOperHeapSize,
                 [&result](WS_SERVICE_PROXY *wsProxyHandle, WS_HEAP *wsHeapHandle, WS_ERROR *wsErrorHandle)
                 {
                     return CalcBindingHeaderAuthSSL_CloseService(wsProxyHandle,
                                                                  &result,
                                                                  wsHeapHandle,
                                                                  nullptr, 0,
                                                                  nullptr,
                                                                  wsErrorHandle);
                 });

            return (result == TRUE);
        }
    };

    /// <summary>
    /// Tests synchronous web service access, with HTTP
    /// header authorization, SSL and a client certificate.
    /// </summary>
    TEST(Framework_WWS_TestCase, Proxy_HeaderAuthTransportSSL_WithClientCert_SyncTest)
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
                AppConfig::GetSettings().application.GetString(keyForCliCertThumbprint, UNDEF_CLIENT_CERT)
            );

            // Create the proxy (client):
            SvcProxyConfig proxyCfg;
            CalcSvcProxyHeaderAuthSSL client(proxyCfg, proxyCertInfo);
            client.Open();

            for (int count = 0; count < 10; ++count)
            {
                EXPECT_EQ(666.0, client.Add(606.0, 60.0));
                EXPECT_EQ(666.0, client.Multiply(111.0, 6.0));
            }

            EXPECT_TRUE(client.CloseHostService());
            client.Close();
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests asynchronous web service access, with HTTP
    /// header authorization, SSL and a client certificate.
    /// </summary>
    TEST(Framework_WWS_TestCase, Proxy_HeaderAuthTransportSSL_WithClientCert_AsyncTest)
    {
        // Ensures proper initialization/finalization of the framework
        _3fd::core::FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            Stall();

            /* Insert here the information describing the client side
            certificate to use in your test environment: */
            SvcProxyCertInfo proxyCertInfo(
                CERT_SYSTEM_STORE_LOCAL_MACHINE,
                "My",
                AppConfig::GetSettings().application.GetString(keyForCliCertThumbprint, UNDEF_CLIENT_CERT)
            );

            // Create the proxy (client):
            SvcProxyConfig proxyCfg;
            CalcSvcProxyHeaderAuthSSL client(proxyCfg, proxyCertInfo);
            client.Open();

            const int maxAsyncCalls = 5;

            std::vector<double> results(maxAsyncCalls);
            std::vector<std::future<void>> asyncOps;
            asyncOps.reserve(maxAsyncCalls);

            // Fire the asynchronous requests:
            for (int idx = 0; idx < maxAsyncCalls; ++idx)
            {
                asyncOps.push_back(
                    client.MultiplyAsync(111.0, 6.0, results[idx])
                );
            }

            // Get the results and check for errors:
            for (int idx = 0; idx < maxAsyncCalls; ++idx)
            {
                asyncOps[idx].get();
                EXPECT_EQ(666.0, results[idx]);
            }

            EXPECT_TRUE(client.CloseHostService());
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

            // Create a proxy (client) without transport security:
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

            /* Insert here the information describing the client
            side certificate to use in your test environment: */
            SvcProxyCertInfo proxyCertInfo(
                CERT_SYSTEM_STORE_LOCAL_MACHINE,
                "My",
                AppConfig::GetSettings().application.GetString(keyForCliCertThumbprint, UNDEF_CLIENT_CERT)
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

            sslClient.Close();

            // Create a secure proxy (client) with HTTP header authentication:
            CalcSvcProxyHeaderAuthSSL headerAuthSslClient(proxyCfg, proxyCertInfo);
            headerAuthSslClient.Open();

            try
            {
                // This request should generate a SOAP fault, hence throwing an exception
                headerAuthSslClient.Multiply(111.0, 6.0);
            }
            catch (IAppException &ex)
            {// Log the fault:
                Logger::Write(ex, Logger::PRIO_ERROR);
            }

            EXPECT_TRUE(headerAuthSslClient.CloseHostService());
            headerAuthSslClient.Close();
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
            Stall();

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
                asyncOp.get();
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
                AppConfig::GetSettings().application.GetString(keyForCliCertThumbprint, UNDEF_CLIENT_CERT)
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
                asyncOp.get();
            }
            catch (IAppException &ex)
            {// Log the fault:
                Logger::Write(ex, Logger::PRIO_ERROR);
            }

            sslClient.Close();

            // Create a secure proxy (client) with HTTP header authentication:
            CalcSvcProxyHeaderAuthSSL headerAuthSslClient(proxyCfg, proxyCertInfo);
            headerAuthSslClient.Open();

            try
            {
                // This request should generate a SOAP fault, hence throwing an exception
                double result;
                auto asyncOp = headerAuthSslClient.MultiplyAsync(111.0, 6.0, result);

                /* The request generates a SOAP fault. This will wait for its
                completion, then it throws an exception made from the deserialized
                SOAP fault response: */
                asyncOp.get();
            }
            catch (IAppException &ex)
            {// Log the fault:
                Logger::Write(ex, Logger::PRIO_ERROR);
            }

            EXPECT_TRUE(headerAuthSslClient.CloseHostService());
            headerAuthSslClient.Close();
        }
        catch (...)
        {
            HandleException();
        }
    }

}// end of namespace integration_tests
}// end of namespace _3fd
