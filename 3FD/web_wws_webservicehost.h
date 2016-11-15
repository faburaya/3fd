#ifndef WEB_WWS_WEBSERVICEHOST_H // header guard
#define WEB_WWS_WEBSERVICEHOST_H

#include "web_wws_utils.h"

#include <map>

namespace _3fd
{
namespace web
{
namespace wws
{
    class BaseSvcEndptBindImpls;

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
		std::map<string, std::shared_ptr<BaseSvcEndptBindImpls>> m_bindNameToImpls;

	public:

		ServiceBindings() {}

		/// <summary>
		/// Initializes a new instance of the <see cref="ServiceBindings"/> struct using move semantics.
		/// </summary>
		/// <param name="ob">The object whose resources will be stolen.</param>
		ServiceBindings(ServiceBindings &&ob) :
            m_bindNameToImpls(std::move(m_bindNameToImpls))
		{}

        std::shared_ptr<BaseSvcEndptBindImpls> GetImplementations(const string &bindName) const;

        void MapBinding(
            const string &bindName,
            const WS_CONTRACT_DESCRIPTION *contractDescription,
            const WS_HTTP_POLICY_DESCRIPTION *policyDescription,
            const void *functionTable
        );

        void MapBinding(
            const string &bindName,
            const WS_CONTRACT_DESCRIPTION *contractDescription,
            const WS_HTTP_SSL_POLICY_DESCRIPTION *policyDescription,
            const void *functionTable
        );

        void MapBinding(
            const string &bindName,
            const WS_CONTRACT_DESCRIPTION *contractDescription,
            const WS_HTTP_SSL_HEADER_AUTH_POLICY_DESCRIPTION *policyDescription,
            const void *functionTable
        );
	};

	/// <summary>
	/// Contains several settings for a service endpoint.
	/// </summary>
	struct SvcEndpointsConfig : public ServiceBindings, notcopiable
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