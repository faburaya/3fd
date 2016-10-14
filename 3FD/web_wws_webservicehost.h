#ifndef WEB_WWS_WEBSERVICEHOST_H // header guard
#define WEB_WWS_WEBSERVICEHOST_H

#include "web_wws_utils.h"

#include <codecvt>
#include <vector>
#include <map>
#include <mutex>
#include <memory>

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
            class BaseSvcEndptBindImpls
            {
            private:

                const void *m_functionTable; // The table of functions that implement the service contract
                WS_CONTRACT_DESCRIPTION *m_contractDescription;

            protected:

                const void *GetFunctionTable() const { return m_functionTable; }

                WS_CONTRACT_DESCRIPTION *GetContractDescription() { return m_contractDescription; }

                virtual void *GetPolicyDescription(WSHeap &heap) = 0;

                virtual size_t GetPolicyDescriptionTypeSize() const = 0;

                virtual WS_CHANNEL_PROPERTIES &GetChannelProperties(WSHeap &heap) = 0;

            public:

                BaseSvcEndptBindImpls(const void *functionTable,
                                      const WS_CONTRACT_DESCRIPTION *contractDescription) :
                    m_functionTable(functionTable),
                    m_contractDescription(const_cast<WS_CONTRACT_DESCRIPTION *> (contractDescription))
                {}

                virtual ~BaseSvcEndptBindImpls() {}

                virtual WS_BINDING_TEMPLATE_TYPE GetBindingTemplateType() const = 0;

                WS_SERVICE_ENDPOINT *CreateWSEndpoint(
                    const string &address,
                    WS_CHANNEL_PROPERTIES &moreChannelProps,
                    WS_SERVICE_ENDPOINT_PROPERTY *endpointProps,
                    size_t endpointPropsCount,
                    WSHeap &heap,
                    WSError &err
                );
            };

            /// <summary>
            /// Holds the implementations (both custom and from wsutil.exe)
            /// for a service endpoint binding "HTTP without transport security".
            /// </summary>
            class SvcEndptBindHttpUnsecImpls : public BaseSvcEndptBindImpls
            {
            private:

                const WS_HTTP_POLICY_DESCRIPTION *m_policyDescription;
                WS_HTTP_POLICY_DESCRIPTION *m_copyOfPolicyDescription;

                virtual void *GetPolicyDescription(WSHeap &heap) override;

                virtual size_t GetPolicyDescriptionTypeSize() const override
                {
                    return sizeof *m_policyDescription;
                }

                virtual WS_CHANNEL_PROPERTIES &GetChannelProperties(WSHeap &heap) override
                {
                    return static_cast<WS_HTTP_POLICY_DESCRIPTION *> (
                        GetPolicyDescription(heap)
                    )->channelProperties;
                }

            public:

                virtual WS_BINDING_TEMPLATE_TYPE GetBindingTemplateType() const override
                {
                    return WS_HTTP_BINDING_TEMPLATE_TYPE;
                }

                SvcEndptBindHttpUnsecImpls(
                    const void *functionTable,
                    const WS_CONTRACT_DESCRIPTION *contractDescription,
                    const WS_HTTP_POLICY_DESCRIPTION *policyDescription
                )
                :   BaseSvcEndptBindImpls(functionTable, contractDescription),
                    m_policyDescription(policyDescription),
                    m_copyOfPolicyDescription(nullptr)
                {}

                virtual ~SvcEndptBindHttpUnsecImpls() {}
            };

            /// <summary>
            /// Holds the implementations (both custom and from wsutil.exe)
            /// for a service endpoint binding "HTTP with SSL on transport".
            /// </summary>
            class SvcEndptBindHttpSslImpls : public BaseSvcEndptBindImpls
            {
            private:

                const WS_HTTP_SSL_POLICY_DESCRIPTION *m_policyDescription;
                WS_HTTP_SSL_POLICY_DESCRIPTION *m_copyOfPolicyDescription;

                virtual void *GetPolicyDescription(WSHeap &heap) override;

                virtual size_t GetPolicyDescriptionTypeSize() const override
                {
                    return sizeof *m_policyDescription;
                }

                virtual WS_CHANNEL_PROPERTIES &GetChannelProperties(WSHeap &heap) override
                {
                    return static_cast<WS_HTTP_POLICY_DESCRIPTION *> (
                        GetPolicyDescription(heap)
                    )->channelProperties;
                }

            public:

                virtual WS_BINDING_TEMPLATE_TYPE GetBindingTemplateType() const override
                {
                    return WS_HTTP_SSL_BINDING_TEMPLATE_TYPE;
                }

                SvcEndptBindHttpSslImpls(
                    const void *functionTable,
                    const WS_CONTRACT_DESCRIPTION *contractDescription,
                    const WS_HTTP_SSL_POLICY_DESCRIPTION *policyDescription
                )
                :   BaseSvcEndptBindImpls(functionTable, contractDescription),
                    m_policyDescription(policyDescription),
                    m_copyOfPolicyDescription(nullptr)
                {}

                virtual ~SvcEndptBindHttpSslImpls() {}
            };

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

                /// <summary>
                /// Gets a set of implementations for a service endpoint with a specific binding.
                /// </summary>
                /// <param name="bindName">Name of the binding.</param>
                /// <returns>
                /// A set of implementations (as function pointers) either from client code
                /// or automatically generated by wsutil.exe.
                /// </returns>
                /// <remarks>
                /// The binding identifier is not fully qualified (by namespace) because this
                /// framework component assumes the programmer is using the target namespace
                /// of prefix 'tns' when declaring the bindings.
                /// </remarks>
                std::shared_ptr<BaseSvcEndptBindImpls> GetImplementations(const string &bindName) const
                {
                    auto iter = m_bindNameToImpls.find(bindName);

                    if (m_bindNameToImpls.end() != iter)
                        return iter->second;
                    else
                        return nullptr;
                }

                /// <summary>
                /// Maps the binding name to the implementations for a
                /// service endpoint binding without transport security.
                /// </summary>
                /// <param name="bindName">Name of the binding.</param>
                /// <param name="contractDescription">The contract description implemented
                /// in the template generated by wsutil.</param>
                /// <param name="policyDescription">The policy description implemented
                /// in the template generated by wsutil.</param>
                /// <param name="functionTable">The table of functions that implement the
                /// service contract as specified in the binding.</param>
                /// <param name="heap">The heap to allocate memory from.</param>
                /// <remarks>
                /// The binding identifier is not fully qualified (by namespace) because this
                /// framework component assumes the programmer is using the target namespace
                /// of prefix 'tns' when declaring the bindings.
                /// </remarks>
                void ServiceBindings::MapBinding(
                    const string &bindName,
                    const WS_CONTRACT_DESCRIPTION *contractDescription,
                    const WS_HTTP_POLICY_DESCRIPTION *policyDescription,
                    const void *functionTable)
                {
                    m_bindNameToImpls[bindName].reset(
                        new SvcEndptBindHttpUnsecImpls(
                            functionTable,
                            contractDescription,
                            policyDescription
                        )
                    );
                }

                /// <summary>
                /// Maps the binding name to the implementations for a
                /// service endpoint binding with SSL on transport.
                /// </summary>
                /// <param name="bindName">Name of the binding.</param>
                /// <param name="contractDescription">The contract description implemented
                /// in the template generated by wsutil.</param>
                /// <param name="policyDescription">The policy description implemented
                /// in the template generated by wsutil.</param>
                /// <param name="functionTable">The table of functions that implement the
                /// service contract as specified in the binding.</param>
                /// <param name="heap">The heap to allocate memory from.</param>
                /// <remarks>
                /// The binding identifier is not fully qualified (by namespace) because this
                /// framework component assumes the programmer is using the target namespace
                /// of prefix 'tns' when declaring the bindings.
                /// </remarks>
                void ServiceBindings::MapBinding(
                    const string &bindName,
                    const WS_CONTRACT_DESCRIPTION *contractDescription,
                    const WS_HTTP_SSL_POLICY_DESCRIPTION *policyDescription,
                    const void *functionTable)
                {
                    m_bindNameToImpls[bindName].reset(
                        new SvcEndptBindHttpSslImpls(
                            functionTable,
                            contractDescription,
                            policyDescription
                        )
                    );
                }

			};// end of class ServiceBindings

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
					maxAcceptingChannels(1),
					maxConcurrency(1),
					timeoutSend(15000),
					timeoutReceive(15000),
					timeoutDnsResolve(60000),
					timeoutClose(0)
				{}
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

				std::shared_ptr<BaseSvcEndptBindImpls> implementations;

				/// <summary>
				/// Initializes a new instance of the <see cref="SvcEndpointInfo"/> struct.
				/// </summary>
				SvcEndpointInfo() {}

				/// <summary>
				/// Initializes a new instance of the <see cref="SvcEndpointInfo"/> struct
				/// using move semantics.
				/// </summary>
				/// <param name="ob">The object whose resources will be stolen.</param>
				SvcEndpointInfo(SvcEndpointInfo &&ob) :
					portName(std::move(ob.portName)),
					bindingName(std::move(ob.bindingName)),
					bindingNs(std::move(ob.bindingNs)),
					address(std::move(ob.address))
				{
                    implementations.swap(ob.implementations);
                }
			};

			/// <summary>
			/// Implements the web service host infrastructure.
			/// </summary>
			class WebServiceHost : notcopiable
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

				WebServiceHost(size_t reservedMemory);

				~WebServiceHost();

				void Setup(
					const string &wsdFilePath,
					const SvcEndpointsConfig &config,
					bool enableMEX);

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
				WS_ERROR *wsErrorHandle);

		}// end of namespace wws
	}// end of namespace web
}// end of namespace _3fd

#endif // end of header guard