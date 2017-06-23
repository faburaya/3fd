#ifndef WEB_WWS_WEBSERVICEHOST_H // header guard
#define WEB_WWS_WEBSERVICEHOST_H

#include "callstacktracer.h"
#include "web_wws_utils.h"
#include <codecvt>
#include <memory>
#include <map>

namespace _3fd
{
namespace web
{
namespace wws
{
    template <typename BindingTemplateType, typename FuncTableType>
    using CreateServiceEndpointImpl = HRESULT (*)(
        _In_opt_ BindingTemplateType *templateValue,
        _In_opt_ CONST WS_STRING *address,
        _In_opt_ FuncTableType *functionTable,
        _In_opt_ WS_SERVICE_SECURITY_CALLBACK authorizationCallback,
        _In_reads_opt_(endpointPropertyCount) WS_SERVICE_ENDPOINT_PROPERTY *endpointProperties,
        _In_ const ULONG endpointPropertyCount,
        _In_ WS_HEAP *heap,
        _Outptr_ WS_SERVICE_ENDPOINT **serviceEndpoint,
        _In_opt_ WS_ERROR *error
    );


    template <typename BindingTemplateType,
              typename FuncTableType,
              CreateServiceEndpointImpl<BindingTemplateType, FuncTableType> callback>
    WS_SERVICE_ENDPOINT *CreateServiceEndpoint(
        BindingTemplateType *bindingTemplate,
        const string &address,
        const void *functionTable,
        WS_CHANNEL_PROPERTIES &channelProps,
        WS_SERVICE_SECURITY_CALLBACK authorizationCallback,
        WS_SERVICE_ENDPOINT_PROPERTY *endpointProps,
        size_t endpointPropsCount,
        WSHeap &heap,
        WSError &err)
    {
        CALL_STACK_TRACE;

        WS_SERVICE_ENDPOINT *wsEndpointHandle;

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto ucs2address = transcoder.from_bytes(address);
        auto wsaddr = heap.Alloc<WS_STRING>();
        wsaddr->length = ucs2address.length();
        wsaddr->chars = heap.Alloc<wchar_t>(ucs2address.length());
        memcpy(wsaddr->chars, ucs2address.data(), ucs2address.length() * sizeof ucs2address[0]);

        bindingTemplate->channelProperties = channelProps;

        HRESULT hr = callback(bindingTemplate,
                              wsaddr,
                              (FuncTableType *)functionTable,
                              authorizationCallback,
                              endpointProps,
                              endpointPropsCount,
                              heap.GetHandle(),
                              &wsEndpointHandle,
                              err.GetHandle());

        if (hr != S_OK)
        {
            std::ostringstream oss;
            oss << "Failed to create web service endpoint at " << address;
            err.RaiseExceptionApiError(hr, "WsCreateServiceEndpointFromTemplate", oss.str().c_str());
        }

        return wsEndpointHandle;
    }


    // This callback declaration suits all implementations of function template CreateServiceEndpoint
    template <typename BindingTemplateType>
    using CallbackCreateServiceEndpoint = WS_SERVICE_ENDPOINT *(*)(
        BindingTemplateType *,
        const string &,
        const void *,
        WS_CHANNEL_PROPERTIES &,
        WS_SERVICE_SECURITY_CALLBACK,
        WS_SERVICE_ENDPOINT_PROPERTY *,
        size_t,
        WSHeap &,
        WSError &
    );


    class BaseSvcEndptBinding;

    /// <summary>
    /// Contains associations of bindings to custom and
    /// automatically generated (by wsutil.exe) implementations.
    /// </summary>
    class ServiceBindings : notcopiable
    {
    private:

        /// <summary>
        /// Maps the binding name to the provided implementation.
        /// </summary>
        std::map<string, std::shared_ptr<BaseSvcEndptBinding>> m_bindNameToImpl;

    public:

        ServiceBindings() {}

        /// <summary>
        /// Initializes a new instance of the <see cref="ServiceBindings"/> struct using move semantics.
        /// </summary>
        /// <param name="ob">The object whose resources will be stolen.</param>
        ServiceBindings(ServiceBindings &&ob) :
            m_bindNameToImpl(std::move(m_bindNameToImpl))
        {}

        std::shared_ptr<BaseSvcEndptBinding> GetImplementation(const string &bindName) const;

        void MapBinding(
            const string &bindName,
            const void *functionTable,
            CallbackCreateServiceEndpoint<WS_HTTP_BINDING_TEMPLATE> callbackCreateSvcEndpt
        );

        void MapBinding(
            const string &bindName,
            const void *functionTable,
            CallbackCreateServiceEndpoint<WS_HTTP_SSL_BINDING_TEMPLATE> callbackCreateSvcEndpt,
            bool requireClientCert
        );

        void MapBinding(
            const string &bindName,
            const void *functionTable,
            CallbackCreateServiceEndpoint<WS_HTTP_SSL_HEADER_AUTH_BINDING_TEMPLATE> callbackCreateSvcEndpt,
            bool requireClientCert
        );
    };


	/// <summary>
	/// Contains several settings for a service endpoint.
	/// </summary>
	struct SvcEndpointsConfig : notcopiable
	{
		unsigned int
			maxAcceptingChannels, // specifies the maximum number of concurrent channels service host will have actively accepting new connections for a given endpoint
			maxConcurrency; // specifies the maximum number of concurrent calls that would be serviced on a session based channel

		unsigned long
			timeoutSend, // limits the amount of time (in milliseconds) that will be spent sending the HTTP headers and the bytes of the message
			timeoutReceive, // limits the amount of time (in milliseconds) that will be spent receiving the the bytes of the message
			timeoutDnsResolve, // limits the amount of time (in milliseconds) that will be spent resolving the DNS name
			timeoutClose; // limits the amount of time (in milliseconds) a service model will wait after 'Close' is called, and once the timeout expires, the host will abort

		/// <summary>
		/// Initializes a new instance of the <see cref="SvcEndpointsConfig"/> struct.
		/// Sets default values for configuration, except mapping of bindings to callbacks.
		/// </summary>
		SvcEndpointsConfig() : 
			maxAcceptingChannels(2),
			maxConcurrency(1),
			timeoutSend(15000),
			timeoutReceive(15000),
			timeoutDnsResolve(60000),
			timeoutClose(0)
		{}
	};


    class WebServiceHostImpl;

	/// <summary>
	/// Implements the web service host infrastructure.
	/// </summary>
	class WebServiceHost : notcopiable
	{
	private:

        WebServiceHostImpl *m_pimpl;

	public:

		WebServiceHost(size_t reservedMemory);

        /// <summary>
        /// Initializes a new instance of the <see cref="WebServiceHost"/> class
        /// using move semantics.
        /// </summary>
        /// <param name="ob">The object whose resources will be stolen.</param>
        WebServiceHost(WebServiceHost &&ob)
            : m_pimpl(ob.m_pimpl)
        {
            ob.m_pimpl = nullptr;
        }

		~WebServiceHost();

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


	//////////////////////
	// Utilities
	//////////////////////

	void SetSoapFault(
		core::IAppException &ex,
		const char *action,
		const WS_OPERATION_CONTEXT *wsOperContextHandle,
		WS_ERROR *wsErrorHandle
    );

    void SetSoapFault(
        const string &reason,
        const string &details,
        const char *action,
        const WS_OPERATION_CONTEXT *wsOperContextHandle,
        WS_ERROR *wsErrorHandle
    );

    bool HelpAuthorizeSender(
        const WS_OPERATION_CONTEXT *wsOperContextHandle,
        HANDLE *senderWinToken,
        WS_ERROR *wsErrorHandle
    );

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd

#endif // end of header guard