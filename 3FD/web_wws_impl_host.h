#ifndef WEB_WWS_HOST_IMPL_H // header guard
#define WEB_WWS_HOST_IMPL_H

#include "web_wws_impl_utils.h"
#include "web_wws_webservicehost.h"

#include <codecvt>
#include <vector>
#include <mutex>

namespace _3fd
{
namespace web
{
namespace wws
{
    /// <summary>
    /// Abstract class that is base to hold the implementations (both custom
    /// and from wsutil.exe) for a service endpoint with a specific binding.
    /// </summary>
    class BaseSvcEndptBinding
    {
    protected:

        const void *m_functionTable; // The table of functions that implement the service contract

    public:

        BaseSvcEndptBinding(const void *functionTable)
            : m_functionTable(functionTable)
        {}

        virtual ~BaseSvcEndptBinding() {}

        virtual WS_SERVICE_ENDPOINT *CreateWSEndpoint(
            const string &address,
            WS_SERVICE_ENDPOINT_PROPERTY *endpointProps,
            size_t endpointPropsCount,
            WS_SERVICE_SECURITY_CALLBACK authzCallback,
            WSHeap &heap,
            WSError &err
        ) = 0;
    };


    /// <summary>
    /// Holds the implementations (both custom and from wsutil.exe)
    /// for a service endpoint binding "HTTP without transport security".
    /// </summary>
    class SvcEndptBindHttpUnsec : public BaseSvcEndptBinding
    {
    private:

        CallbackCreateServiceEndpoint<WS_HTTP_BINDING_TEMPLATE> m_callbackCreateSvcEndpt;

    public:


        SvcEndptBindHttpUnsec(const void *functionTable,
                              CallbackCreateServiceEndpoint<WS_HTTP_BINDING_TEMPLATE> callbackCreateSvcEndpt)
            : BaseSvcEndptBinding(functionTable)
            , m_callbackCreateSvcEndpt(callbackCreateSvcEndpt)
        {}

        virtual ~SvcEndptBindHttpUnsec() {}

        virtual WS_SERVICE_ENDPOINT *CreateWSEndpoint(
            const string &address,
            WS_SERVICE_ENDPOINT_PROPERTY *endpointProps,
            size_t endpointPropsCount,
            WS_SERVICE_SECURITY_CALLBACK authzCallback,
            WSHeap &heap,
            WSError &err
        ) override;
    };


    /// <summary>
    /// Holds the implementations (both custom and from wsutil.exe)
    /// for a service endpoint binding "HTTP with SSL on transport".
    /// </summary>
    class SvcEndptBindHttpSsl : public BaseSvcEndptBinding
    {
    private:

        CallbackCreateServiceEndpoint<WS_HTTP_SSL_BINDING_TEMPLATE> m_callbackCreateSvcEndpt;

        bool m_clientCertIsRequired;

    public:

        SvcEndptBindHttpSsl(const void *functionTable,
                            CallbackCreateServiceEndpoint<WS_HTTP_SSL_BINDING_TEMPLATE> callbackCreateSvcEndpt,
                            bool requireClientCert)
            : BaseSvcEndptBinding(functionTable)
            , m_callbackCreateSvcEndpt(callbackCreateSvcEndpt)
            , m_clientCertIsRequired(requireClientCert)
        {}

        virtual ~SvcEndptBindHttpSsl() {}

        virtual WS_SERVICE_ENDPOINT *CreateWSEndpoint(
            const string &address,
            WS_SERVICE_ENDPOINT_PROPERTY *endpointProps,
            size_t endpointPropsCount,
            WS_SERVICE_SECURITY_CALLBACK authzCallback,
            WSHeap &heap,
            WSError &err
        ) override;
    };


    /// <summary>
    /// Holds the implementations (both custom and from wsutil.exe) for a service
    /// endpoint binding "HTTP header authentication with SSL on transport".
    /// </summary>
    class SvcEndptBindHttpHeaderAuthSsl : public BaseSvcEndptBinding
    {
    private:

        CallbackCreateServiceEndpoint<WS_HTTP_SSL_HEADER_AUTH_BINDING_TEMPLATE> m_callbackCreateSvcEndpt;

        bool m_clientCertIsRequired;

    public:

        SvcEndptBindHttpHeaderAuthSsl(const void *functionTable,
                                      CallbackCreateServiceEndpoint<WS_HTTP_SSL_HEADER_AUTH_BINDING_TEMPLATE> callbackCreateSvcEndpt,
                                      bool requireClientCert)
            : BaseSvcEndptBinding(functionTable)
            , m_callbackCreateSvcEndpt(callbackCreateSvcEndpt)
            , m_clientCertIsRequired(requireClientCert)
        {}

        virtual ~SvcEndptBindHttpHeaderAuthSsl() {}

        virtual WS_SERVICE_ENDPOINT *CreateWSEndpoint(
            const string &address,
            WS_SERVICE_ENDPOINT_PROPERTY *endpointProps,
            size_t endpointPropsCount,
            WS_SERVICE_SECURITY_CALLBACK authzCallback,
            WSHeap &heap,
            WSError &err
        ) override;
    };


    /// <summary>
    /// Holds key content for creation and descrition of a service endpoint.
    /// </summary>
    struct SvcEndpointInfo : notcopiable
    {
        string
            portName,
            bindingName,
            bindingNs,
            address;

        std::shared_ptr<BaseSvcEndptBinding> implementations;

        /// <summary>
        /// Initializes a new instance of the <see cref="SvcEndpointInfo"/> struct.
        /// </summary>
        SvcEndpointInfo() {}

        /// <summary>
        /// Initializes a new instance of the <see cref="SvcEndpointInfo"/> struct
        /// using move semantics.
        /// </summary>
        /// <param name="ob">The object whose resources will be stolen.</param>
        SvcEndpointInfo(SvcEndpointInfo &&ob) noexcept
            : portName(std::move(ob.portName))
            , bindingName(std::move(ob.bindingName))
            , bindingNs(std::move(ob.bindingNs))
            , address(std::move(ob.address))
        {
            implementations.swap(ob.implementations);
        }
    };


    /// <summary>
    /// Implements the web service host infrastructure.
    /// </summary>
    class WebServiceHostImpl : notcopiable
    {
    private:

        WS_SERVICE_HOST *m_wsSvcHostHandle;
        std::vector<char> m_wsdContentBuffer;
        string m_wsdTargetNs;
        string m_serviceName;
        std::vector<SvcEndpointInfo> m_endpointsInfo;
        std::mutex m_hostStateMutex;
        WSHeap m_svcHeap;

    public:

        WebServiceHostImpl(size_t reservedMemory);

        ~WebServiceHostImpl();

        void Setup(
            const string &wsdFilePath,
            const SvcEndpointsConfig &config,
            const ServiceBindings &bindings,
            WS_SERVICE_SECURITY_CALLBACK authzCallback,
            bool enableMEX
        );

        void Open();
        bool Close();
        bool Abort();
    };

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd

#endif // end of header guard
