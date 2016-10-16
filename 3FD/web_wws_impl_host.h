#ifndef WEB_WWS_HOST_IMPL_H // header guard
#define WEB_WWS_HOST_IMPL_H

#include "web_wws_impl_utils.h"
#include "web_wws_webservicehost.h"

#include <codecvt>
#include <vector>
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
                    WS_SERVICE_SECURITY_CALLBACK authzCallback,
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
            /// Holds the implementations (both custom and from wsutil.exe) for a service
            /// endpoint binding "HTTP header authentication with SSL on transport".
            /// </summary>
            class SvcEndptBindHttpHeaderAuthSslImpls : public BaseSvcEndptBindImpls
            {
            private:

                const WS_HTTP_SSL_HEADER_AUTH_POLICY_DESCRIPTION *m_policyDescription;
                WS_HTTP_SSL_HEADER_AUTH_POLICY_DESCRIPTION *m_copyOfPolicyDescription;

                virtual void *GetPolicyDescription(WSHeap &heap) override;

                virtual size_t GetPolicyDescriptionTypeSize() const override
                {
                    return sizeof *m_policyDescription;
                }

                virtual WS_CHANNEL_PROPERTIES &GetChannelProperties(WSHeap &heap) override
                {
                    return static_cast<WS_HTTP_SSL_HEADER_AUTH_POLICY_DESCRIPTION *> (
                        GetPolicyDescription(heap)
                    )->channelProperties;
                }

            public:

                virtual WS_BINDING_TEMPLATE_TYPE GetBindingTemplateType() const override
                {
                    return WS_HTTP_SSL_HEADER_AUTH_BINDING_TEMPLATE_TYPE;
                }

                SvcEndptBindHttpHeaderAuthSslImpls(
                    const void *functionTable,
                    const WS_CONTRACT_DESCRIPTION *contractDescription,
                    const WS_HTTP_SSL_HEADER_AUTH_POLICY_DESCRIPTION *policyDescription
                )
                :   BaseSvcEndptBindImpls(functionTable, contractDescription),
                    m_policyDescription(policyDescription),
                    m_copyOfPolicyDescription(nullptr)
                {}

                virtual ~SvcEndptBindHttpHeaderAuthSslImpls() {}
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
