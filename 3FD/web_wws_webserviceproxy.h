#ifndef WEB_WWS_WEBSERVICEPROXY_H // header guard
#define WEB_WWS_WEBSERVICEPROXY_H

#include "web_wws_utils.h"

namespace _3fd
{
namespace web
{
namespace wws
{
	/// <summary>
	/// Holds some configurations for the service proxy.
	/// </summary>
	struct SvcProxyConfig
	{
		size_t reservedMemory; // The amount of memory to allocate for the proxy setup (in bytes)

		unsigned long
			timeoutDnsResolve, // limits the amount of time (in milliseconds) that will be spent resolving the DNS name
			timeoutSend, // limits the amount of time (in milliseconds) that will be spent sending the HTTP headers and the bytes of the message
			timeoutReceive, // limits the amount of time (in milliseconds) that will be spent receiving the the bytes of the message
			timeoutCall, // The maximum amount of time in milliseconds for a call to remain pending.
			timeoutClose; /* The amount of time in milliseconds the service proxy will wait for the pending calls
								to complete. Once the timeout expires, the service proxy will abort itself. */
		SvcProxyConfig() : 
			reservedMemory(1024),
			timeoutDnsResolve(60000),
			timeoutSend(15000),
			timeoutReceive(15000),
			timeoutCall(15000),
			timeoutClose(5000)
		{}
	};

	/// <summary>
	/// Holds information to describe a certificate to use for SSL in the service proxy.
	/// </summary>
	struct SvcProxyCertInfo : notcopiable
	{
		/// <summary>
		/// The certificate store location (such as CERT_SYSTEM_STORE_CURRENT_USER
		/// or CERT_SYSTEM_STORE_LOCAL_MACHINE) that contains the specified certificate.
		/// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa388136.aspx.
		/// </summary>
		unsigned long storeLocation;

		/// <summary>
		/// The certificate store name (such as "My")
		/// that contains the specified certificate.
		/// </summary>
		string storeName;

		/// <summary>
		/// The SHA-1 thumbprint (such as "c0f89c8d4e4e80f250b58c3fae944a0edee02804")
		/// of the specified certificate. The supplied value should be a hexadecimal
		/// string without whitespace characters or a leading 0x.
		/// </summary>
		string thumbprint;

		/// <summary>
		/// Initializes a new instance of the <see cref="SvcProxyCertInfo"/> struct.
		/// </summary>
		/// <param name="p_storeLocation">The certificate store location.</param>
		/// <param name="storeName">Name of the store.</param>
		/// <param name="thumbprint">The certificate thumbprint.</param>
		SvcProxyCertInfo(unsigned long p_storeLocation,
					     const string &p_storeName,
				         const string &p_thumbprint) : 
			storeLocation(p_storeLocation),
			storeName(p_storeName),
			thumbprint(p_thumbprint)
		{}
	};

    /* Callback type that invokes the creation of a web service proxy
    as implemented by code generated by wsutil.exe */
	template <typename BindingTemplateType>
	using CallbackCreateServiceProxyImpl = HRESULT(*)(
        _In_opt_ BindingTemplateType* templateValue,
        _In_reads_opt_(proxyPropertyCount) const WS_PROXY_PROPERTY* proxyProperties,
        _In_ const ULONG proxyPropertyCount,
        _Outptr_ WS_SERVICE_PROXY** _serviceProxy,
        _In_opt_ WS_ERROR* error
    );

    /// <summary>
    /// Wraps code for creation of a proxy (client) for the web service.
    /// </summary>
    /// <param name="properties">The proxy properties.</param>
    /// <param name="propCount">The counting of properties.</param>
    /// <param name="wsProxyHandle">Where to save the created proxy handle.</param>
    /// <param name="err">Where to keep rich error information.</param>
    /// <returns>
    /// An HRESULT code, which is 'S_OK' when successful.
    /// </returns>
    template <typename BindingTemplateType,
              CallbackCreateServiceProxyImpl<BindingTemplateType> callback>
    HRESULT CreateWSProxy(
        WS_CHANNEL_PROPERTIES channelProperties,
        const WS_PROXY_PROPERTY *proxyProperties,
        size_t proxyPropCount,
        WS_SERVICE_PROXY **wsSvcProxyHandle,
        WSHeap &heap,
        WSError &err)
    {
        auto bindingTemplate = WS_HEAP_NEW(heap, BindingTemplateType, {});
        bindingTemplate->channelProperties = channelProperties;

        return callback(
            bindingTemplate,
            proxyProperties,
            proxyPropCount,
            wsSvcProxyHandle,
            err.GetHandle()
        );
    }

	/* Callback type for template function that wraps code
	automatically generated by wsutil.exe */
	typedef HRESULT(*CallbackWrapperCreateServiceProxy)(
		WS_CHANNEL_PROPERTIES,
		const WS_PROXY_PROPERTY *, // proxy properties
		size_t, // count of proxy properties
		WS_SERVICE_PROXY **,
		WSHeap &,
		WSError &
	);

    class WebServiceProxyImpl;

    /// <summary>
    /// Represents a proxy for a running web service host.
    /// </summary>
    class WebServiceProxy : notcopiable
    {
    private:

        WebServiceProxyImpl *m_pimpl;

    protected:

        WS_SERVICE_PROXY *GetHandle() const;

    public:

		WebServiceProxy(const string &svcEndpointAddress,
						const SvcProxyConfig &config,
						CallbackWrapperCreateServiceProxy callback);

		WebServiceProxy(
			const string &svcEndpointAddress,
			const SvcProxyConfig &config,
			const SvcProxyCertInfo &certInfo,
			CallbackCreateServiceProxyImpl<WS_HTTP_SSL_BINDING_TEMPLATE> callback
		);

        WebServiceProxy(
            const string &svcEndpointAddress,
            const SvcProxyConfig &config,
            const SvcProxyCertInfo &certInfo,
            CallbackCreateServiceProxyImpl<WS_HTTP_SSL_HEADER_AUTH_BINDING_TEMPLATE> callback
        );

        /// <summary>
        /// Initializes a new instance of the <see cref="WebServiceProxy"/> class
        /// using move semantics.
        /// </summary>
        /// <param name="ob">The ob.</param>
        WebServiceProxy(WebServiceProxy &&ob)
            : m_pimpl(ob.m_pimpl)
        {
            ob.m_pimpl = nullptr;
        }

        ~WebServiceProxy();

		WSAsyncOper CreateAsyncOperation(size_t heapSize);

        void Open();
        bool Close();
        bool Abort();
    };

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd

#endif // end of header guard
