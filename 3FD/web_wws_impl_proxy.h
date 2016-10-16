#ifndef WEB_WWS_PROXY_IMPL_H // header guard
#define WEB_WWS_PROXY_IMPL_H

#include "web_wws_webserviceproxy.h"
#include "web_wws_impl_utils.h"

#include <mutex>
#include <vector>

namespace _3fd
{
namespace web
{
namespace wws
{
    /// <summary>
    /// Represents a proxy for a running web service host.
    /// </summary>
    class WebServiceProxyImpl : notcopiable
    {
    private:

        WS_SERVICE_PROXY *m_wsSvcProxyHandle;

        std::wstring m_svcEndptAddr;

        std::mutex m_proxyStateMutex;

        WSHeap m_heap;

        std::vector<std::promise<HRESULT> *> m_promises;

    public:

        WebServiceProxyImpl(
            const string &svcEndpointAddress,
            const SvcProxyConfig &config,
            CallbackWrapperCreateServiceProxy callback
        );

        WebServiceProxyImpl(
            const string &svcEndpointAddress,
            const SvcProxyConfig &config,
            const SvcProxyCertInfo &certInfo,
            CallbackCreateServiceProxyImpl<WS_HTTP_SSL_BINDING_TEMPLATE> callback
        );

        WebServiceProxyImpl(
            const string &svcEndpointAddress,
            const SvcProxyConfig &config,
            const SvcProxyCertInfo &certInfo,
            CallbackCreateServiceProxyImpl<WS_HTTP_SSL_HEADER_AUTH_BINDING_TEMPLATE> callback
        );

        ~WebServiceProxyImpl();

        WSAsyncOper CreateAsyncOperation(size_t heapSize);

        /// <summary>
        /// Gets the handle for this web service proxy.
        /// </summary>
        /// <returns>A handle for this web service proxy.</returns>
        WS_SERVICE_PROXY *GetHandle() const { return m_wsSvcProxyHandle; }

        void Open();
        bool Close();
        bool Abort();
    };

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd

#endif // end of header guard
