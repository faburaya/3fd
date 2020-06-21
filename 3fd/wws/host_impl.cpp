#include "pch.h"
#include "host_impl.h"

#include <3fd/core/callstacktracer.h>
#include <3fd/core/logger.h>
#include <3fd/utils/xml.h>
#include <array>
#include <fstream>
#include <functional>
#include <list>
#include <sstream>

namespace _3fd
{
namespace web
{
namespace wws
{
    using namespace _3fd::core;

    /////////////////////////////////////////////
    // BaseSvcEndptBinding & Derived Classes
    /////////////////////////////////////////////

    /// <summary>
    /// Creates the service endpoint in a given address
    /// using the held implementations.
    /// </summary>
    /// <param name="address">The endpoint URL address.</param>
    /// <param name="endpointProps">The endpoint properties.</param>
    /// <param name="endpointPropsCount">The counting of endpoint properties.</param>
    /// <param name="authzCallback">The callback for authorization.</param>
    /// <param name="heap">The heap to allocate memory from.</param>
    /// <param name="err">Where to keep rich error information.</param>
    /// <returns>The handle for the newly created service endpoint.</returns>
    WS_SERVICE_ENDPOINT *
    SvcEndptBindHttpUnsec::CreateWSEndpoint(const string &address,
                                            WS_SERVICE_ENDPOINT_PROPERTY *endpointProps,
                                            size_t endpointPropsCount,
                                            WS_SERVICE_SECURITY_CALLBACK authzCallback,
                                            WSHeap &heap,
                                            WSError &err)
    {
        CALL_STACK_TRACE;

        WS_HTTP_BINDING_TEMPLATE bindingTemplate{};

        return m_callbackCreateSvcEndpt(&bindingTemplate,
                                        address,
                                        m_functionTable,
                                        authzCallback,
                                        endpointProps,
                                        endpointPropsCount,
                                        heap,
                                        err);
    }


    /// <summary>
    /// Creates the service endpoint in a given address
    /// using the held implementations.
    /// </summary>
    /// <param name="address">The endpoint URL address.</param>
    /// <param name="endpointProps">The endpoint properties.</param>
    /// <param name="endpointPropsCount">The counting of endpoint properties.</param>
    /// <param name="authzCallback">The callback for authorization.</param>
    /// <param name="heap">The heap to allocate memory from.</param>
    /// <param name="err">Where to keep rich error information.</param>
    /// <returns>The handle for the newly created service endpoint.</returns>
    WS_SERVICE_ENDPOINT *
    SvcEndptBindHttpSsl::CreateWSEndpoint(const string &address,
                                          WS_SERVICE_ENDPOINT_PROPERTY *endpointProps,
                                          size_t endpointPropsCount,
                                          WS_SERVICE_SECURITY_CALLBACK authzCallback,
                                          WSHeap &heap,
                                          WSError &err)
    {
        CALL_STACK_TRACE;

        WS_HTTP_SSL_BINDING_TEMPLATE bindingTemplate{};

        // Extend security binding properties:
        auto &bindSecProps = bindingTemplate.sslTransportSecurityBinding.securityBindingProperties;
        bindSecProps.propertyCount = 1;
        bindSecProps.properties = heap.Alloc<WS_SECURITY_BINDING_PROPERTY>();

        bindSecProps.properties[0] =
            WS_SECURITY_BINDING_PROPERTY{
                WS_SECURITY_BINDING_PROPERTY_REQUIRE_SSL_CLIENT_CERT,
                new (heap.Alloc<BOOL>()) BOOL(m_clientCertIsRequired ? TRUE : FALSE),
                sizeof(BOOL)
            };

        return m_callbackCreateSvcEndpt(&bindingTemplate,
                                        address,
                                        m_functionTable,
                                        authzCallback,
                                        endpointProps,
                                        endpointPropsCount,
                                        heap,
                                        err);
    }


    /// <summary>
    /// Creates the service endpoint in a given address
    /// using the held implementations.
    /// </summary>
    /// <param name="address">The endpoint URL address.</param>
    /// <param name="endpointProps">The endpoint properties.</param>
    /// <param name="endpointPropsCount">The counting of endpoint properties.</param>
    /// <param name="authzCallback">The callback for authorization.</param>
    /// <param name="heap">The heap to allocate memory from.</param>
    /// <param name="err">Where to keep rich error information.</param>
    /// <returns>The handle for the newly created service endpoint.</returns>
    WS_SERVICE_ENDPOINT *
    SvcEndptBindHttpHeaderAuthSsl::CreateWSEndpoint(const string &address,
                                                    WS_SERVICE_ENDPOINT_PROPERTY *endpointProps,
                                                    size_t endpointPropsCount,
                                                    WS_SERVICE_SECURITY_CALLBACK authzCallback,
                                                    WSHeap &heap,
                                                    WSError &err)
    {
        CALL_STACK_TRACE;

        WS_HTTP_SSL_HEADER_AUTH_BINDING_TEMPLATE bindingTemplate{};

        // Extend security binding properties:
        auto &bindSecProps = bindingTemplate.sslTransportSecurityBinding.securityBindingProperties;
        bindSecProps.propertyCount = 1;
        bindSecProps.properties = heap.Alloc<WS_SECURITY_BINDING_PROPERTY>();

        bindSecProps.properties[0] =
            WS_SECURITY_BINDING_PROPERTY{
                WS_SECURITY_BINDING_PROPERTY_REQUIRE_SSL_CLIENT_CERT,
                new (heap.Alloc<BOOL>()) BOOL(m_clientCertIsRequired ? TRUE : FALSE),
                sizeof(BOOL)
            };

        return m_callbackCreateSvcEndpt(&bindingTemplate,
                                        address,
                                        m_functionTable,
                                        authzCallback,
                                        endpointProps,
                                        endpointPropsCount,
                                        heap,
                                        err);
    }


    /////////////////////////////
    // ServiceBindings Class
    /////////////////////////////

    /// <summary>
    /// Gets a set of implementations for a service endpoint with a specific binding.
    /// </summary>
    /// <param name="bindName">Name of the binding.</param>
    /// <returns>
    /// An object that holds the implementation automatically generated by wsutil.exe
    /// which is responsible for creating the endpoint for the given binding.
    /// </returns>
    /// <remarks>
    /// The binding identifier is not fully qualified (by namespace) because this
    /// framework component assumes the programmer is using the target namespace
    /// of prefix 'tns' when declaring the bindings.
    /// </remarks>
    std::shared_ptr<BaseSvcEndptBinding> ServiceBindings::GetImplementation(const string &bindName) const
    {
        auto iter = m_bindNameToImpl.find(bindName);

        if (m_bindNameToImpl.end() != iter)
            return iter->second;
        else
            return nullptr;
    }


    /// <summary>
    /// Maps the binding name to the implementations for a
    /// service endpoint binding without transport security.
    /// </summary>
    /// <param name="bindName">Name of the binding.</param>
    /// <param name="functionTable">The table of functions that implement the
    /// service contract as specified in the binding.</param>
    /// <param name="callbackCreateSvcEndpt">A callback originated from the
    /// implementation generated by wsutil.exe, which detains the role of
    /// creating the endpoint for the given binding.</param>
    /// <remarks>
    /// The binding identifier is not fully qualified (by namespace) because this
    /// framework component assumes the programmer is using the target namespace
    /// of prefix 'tns' when declaring the bindings.
    /// </remarks>
    void ServiceBindings::MapBinding(
        const string &bindName,
        const void *functionTable,
        CallbackCreateServiceEndpoint<WS_HTTP_BINDING_TEMPLATE> callbackCreateSvcEndpt)
    {
        m_bindNameToImpl[bindName].reset(
            dbg_new SvcEndptBindHttpUnsec(functionTable, callbackCreateSvcEndpt)
        );
    }


    /// <summary>
    /// Maps the binding name to the implementations for a
    /// service endpoint binding "HTTP with SSL on transport".
    /// </summary>
    /// <param name="bindName">Name of the binding.</param>
    /// <param name="functionTable">The table of functions that implement the
    /// service contract as specified in the binding.</param>
    /// <param name="callbackCreateSvcEndpt">A callback originated from the
    /// implementation generated by wsutil.exe, which detains the role of
    /// creating the endpoint for the given binding.</param>
    /// <param name="requireClientCert">Whether the binding requires the client
    /// to provide a certificate.</param>
    /// <remarks>
    /// The binding identifier is not fully qualified (by namespace) because this
    /// framework component assumes the programmer is using the target namespace
    /// of prefix 'tns' when declaring the bindings.
    /// </remarks>
    void ServiceBindings::MapBinding(
        const string &bindName,
        const void *functionTable,
        CallbackCreateServiceEndpoint<WS_HTTP_SSL_BINDING_TEMPLATE> callbackCreateSvcEndpt,
        bool requireClientCert)
    {
        m_bindNameToImpl[bindName].reset(
            dbg_new SvcEndptBindHttpSsl(
                functionTable,
                callbackCreateSvcEndpt,
                requireClientCert
            )
        );
    }


    /// <summary>
    /// Maps the binding name to the implementations for a service
    /// endpoint binding "HTTP header authentication with SSL on transport".
    /// </summary>
    /// <param name="bindName">Name of the binding.</param>
    /// <param name="functionTable">The table of functions that implement the
    /// service contract as specified in the binding.</param>
    /// <param name="callbackCreateSvcEndpt">A callback originated from the
    /// implementation generated by wsutil.exe, which detains the role of
    /// creating the endpoint for the given binding.</param>
    /// <param name="requireClientCert">Whether the binding requires the client
    /// to provide a certificate.</param>
    /// <remarks>
    /// The binding identifier is not fully qualified (by namespace) because this
    /// framework component assumes the programmer is using the target namespace
    /// of prefix 'tns' when declaring the bindings.
    /// </remarks>
    void ServiceBindings::MapBinding(
        const string &bindName,
        const void *functionTable,
        CallbackCreateServiceEndpoint<WS_HTTP_SSL_HEADER_AUTH_BINDING_TEMPLATE> callbackCreateSvcEndpt,
        bool requireClientCert)
    {
        m_bindNameToImpl[bindName].reset(
            dbg_new SvcEndptBindHttpHeaderAuthSsl(
                functionTable,
                callbackCreateSvcEndpt,
                requireClientCert
            )
        );
    }


    /////////////////////////////
    // WebServiceHost Class
    /////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="WebServiceHostImpl" /> class.
    /// </summary>
    /// <param name="reservedMemory">
    /// The amount (in bytes) of memory to reserve for the service host setup.
    /// </param>
    WebServiceHostImpl::WebServiceHostImpl(size_t reservedMemory)
    try :
        m_wsSvcHostHandle(nullptr),
        m_svcHeap(reservedMemory),
        m_hostStateMutex()
    {
    }
    catch (IAppException &ex)
    {
        CALL_STACK_TRACE;
        throw AppException<std::runtime_error>(
            "Failed to instantiate wrapper object for web service host", ex
        );
    }
    catch (std::system_error &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "System failure when creating web service host: " << StdLibExt::GetDetailsFromSystemError(ex);
        throw AppException<std::runtime_error>(oss.str());
    }
    catch (std::exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Generic failure when creating web service host: " << ex.what();
        throw AppException<std::runtime_error>(oss.str());
    }


    /// <summary>
    /// Initializes a new instance of the <see cref="WebServiceHostImpl" /> class.
    /// </summary>
    /// <param name="reservedMemory">
    /// The amount (in bytes) of memory to reserve for the service host setup.
    /// </param>
    WebServiceHost::WebServiceHost(size_t reservedMemory) :
        m_pimpl(nullptr)
    {
        CALL_STACK_TRACE;

        try
        {
            m_pimpl = dbg_new WebServiceHostImpl(reservedMemory);
        }
        catch (std::bad_alloc &)
        {
            throw AppException<std::runtime_error>(
                "Failed to allocate memory for instantiation of web service host"
            );
        }
    }


    /// <summary>
    /// Finalizes an instance of the <see cref="WebServiceHostImpl"/> class.
    /// </summary>
    WebServiceHostImpl::~WebServiceHostImpl()
    {
        if (m_wsSvcHostHandle == nullptr)
            return;

        CALL_STACK_TRACE;

        try
        {
            Abort();
            WsFreeServiceHost(m_wsSvcHostHandle);
        }
        catch (IAppException &ex)
        {
            Logger::Write(ex, Logger::Priority::PRIO_CRITICAL);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when terminating web service host: " << ex.what();
            Logger::Write(oss.str(), Logger::Priority::PRIO_CRITICAL);
        }
    }


    WebServiceHost::~WebServiceHost()
    {
        delete m_pimpl;
    }


    /// <summary>
    /// Parses information about endpoints from a WSDL document.
    /// </summary>
    /// <param name="wsdRootNode">Root node of the web service definition loaded from a file.</param>
    /// <param name="bindings">The bindings assigned to the endpoints.</param>
    /// <param name="targetNamespace">Where to save the target namespace.</param>
    /// <param name="serviceName">Where to save the service name.</param>
    /// <param name="endpointsAddrs">Where to save the addresses of the endpoints.</param>
    /// <remarks>
    /// Assumes the usage of HTTP & SOAP, but does not check if the document is thoroughly
    /// well formed. Because this library is meant to integrate with wsutil.exe generated code,
    /// the WSDL document is expected to follow the specification in http://www.w3.org/TR/wsdl.
    /// </remarks>
    static void ParseEndpointsFromWSD(
        const rapidxml::xml_node<char> *wsdRootNode,
        const ServiceBindings &bindings,
        string &targetNamespace,
        string &serviceName,
        std::vector<SvcEndpointInfo> &endpointsInfo)
    {
        CALL_STACK_TRACE;

        try
        {
            // If anything goes wrong, do not keep any previous content in the output:
            targetNamespace.clear();
            serviceName.clear();
            endpointsInfo.clear();

            // Fundamental namespaces URI's:
            xml::NamespaceResolver nsResolver;
            nsResolver.LoadNamespacesFrom(wsdRootNode);
            nsResolver.AddAliasForNsPrefix("wsdl", "http://schemas.xmlsoap.org/wsdl/");

            const rapidxml::xml_node<char> *elementPort(nullptr);

            auto query = xml::QueryElement("wsdl:definitions", xml::Required, {
                xml::QueryAttribute("targetNamespace", xml::Required, xml::parse_into(targetNamespace)),
                xml::QueryElement("wsdl:service", xml::Required, {
                    xml::QueryAttribute("name", xml::Required, xml::parse_into(serviceName)),
                    xml::QueryElement("wsdl:port", xml::Required, {}, &elementPort)
                })
            });

            if (!query->Execute(wsdRootNode,
                                xml::QueryStrategy::TestsOnlyGivenElement,
                                &nsResolver))
            {
                std::ostringstream oss;
                oss << "Could not match XML query looking for" _newLine_ _newLine_;
                query->SerializeTo(2, oss);
                oss << _newLine_ "where:" _newLine_ _newLine_;
                nsResolver.SerializeTo(0, oss);

                throw AppException<std::runtime_error>("Web service definition is not compliant", oss.str());
            }

            // find SOAP version:
            for (auto soapVersion : { "http://schemas.xmlsoap.org/wsdl/soap12/",
                                      "http://schemas.xmlsoap.org/wsdl/soap11/",
                                      "http://schemas.xmlsoap.org/wsdl/soap/" })
            {
                if (nsResolver.Has(soapVersion))
                {
                    nsResolver.AddAliasForNsPrefix("soap", soapVersion);
                    break;
                }
            }

            SvcEndpointInfo endpoint;

            std::string qualBindName;

            query = xml::QueryElement("wsdl:port", xml::Required, {
                xml::QueryAttribute("name", xml::Required, xml::parse_into(endpoint.portName)),
                xml::QueryAttribute("binding", xml::Required, xml::parse_into(qualBindName)),
                xml::QueryElement("soap:address", xml::Required, {
                    xml::QueryAttribute("location", xml::Required, xml::parse_into(endpoint.address))
                })
            });

            // Iterate over each endpoint specification:
            do
            {
                if (!query->Execute(elementPort,
                                    xml::QueryStrategy::TestsOnlyGivenElement,
                                    &nsResolver))
                {
                    std::ostringstream oss;
                    oss << "Port for service '" << serviceName
                        << "' could not match XML query looking for" _newLine_ _newLine_;
                    query->SerializeTo(2, oss);
                    oss << _newLine_ "where:" _newLine_ _newLine_;
                    nsResolver.SerializeTo(0, oss);

                    throw AppException<std::runtime_error>("Web service definition is not compliant", oss.str());
                }

                if (!nsResolver.ParseQualifiedName(qualBindName,
                                                   endpoint.bindingNs,
                                                   endpoint.bindingName))
                {
                    std::ostringstream oss;
                    oss << "Could not resolve WSDL binding '" << qualBindName
                        << "' for port in service '" << serviceName << '\'';

                    throw AppException<std::runtime_error>("Web service definition is not compliant", oss.str());
                }
                        
                // For the assigned binding, get the implementations
                auto svcForBind = bindings.GetImplementation(endpoint.bindingName);

                if (svcForBind.get() != nullptr)
                {
                    endpoint.implementations.swap(svcForBind);
                    endpointsInfo.push_back(std::move(endpoint));
                }
                else
                {// If no implementation has been found for the endpoint binding:
                    std::ostringstream oss;
                    oss << "The implementation sets provided for endpoint bindings "
                            "had no match for port '" << endpoint.portName
                        << "' with assigned binding '" << endpoint.bindingName
                        << "' in service '" << serviceName
                        << "', hence this endpoint cannot be created";

                    Logger::Write(oss.str(), Logger::PRIO_NOTICE);
                }

                elementPort = xml::GetNextSiblingOf(elementPort,
                                                    xml::xstr("wsdl:port"),
                                                    &nsResolver);
            } while (elementPort != nullptr);

            if (endpointsInfo.empty())
            {
                throw AppException<std::runtime_error>(
                    "No endpoints could be created from the provided WSDL "
                    "and mapped implementations for bindings");
            }
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions regarding errors known to have been previously handled
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when parsing web service definition: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Creates the endpoints for a web service.
    /// </summary>
    /// <param name="config">The configurations common to all endpoints in the web service.</param>
    /// <param name="endptsInfo">The information for each of the endpoints to create.</param>
    /// <param name="authzCallback">The callback for authorization.</param>
    /// <param name="enableMEX">If set to <c>true</c>, the endpoint will serve metadata
    /// in the endpoints using WS-MetadataExchange requests.</param>
    /// <param name="endpoints">Where to save the endpoints to be created.</param>
    /// <param name="heap">The heap that provides memory allocation.</param>
    static void CreateWebSvcEndpoints(
        const SvcEndpointsConfig &config,
        const std::vector<SvcEndpointInfo> &endptsInfo,
        WS_SERVICE_SECURITY_CALLBACK authzCallback,
        bool enableMEX,
        std::vector<const WS_SERVICE_ENDPOINT *> &endpoints,
        WSHeap &heap)
    {
        CALL_STACK_TRACE;

        try
        {
            endpoints.clear(); // if anything goes wrong, do not keep any previous content in the output

            /* Set the endpoint properties from the settings in the parameters,
            starting by the ones regarding metadata servicing: */

            WS_SERVICE_ENDPOINT_PROPERTY *endpointProps;

            uint32_t idxProp(0);
            uint32_t endptPropCount(3); // count of properties is later needed

            if (enableMEX)
            {
                endptPropCount += 2; // count of properties is later needed
                endpointProps = heap.Alloc<WS_SERVICE_ENDPOINT_PROPERTY>(endptPropCount);

                // Enable servicing of WS-MetadataExchange requests:
                endpointProps[idxProp++] = WS_SERVICE_ENDPOINT_PROPERTY {
                    WS_SERVICE_ENDPOINT_PROPERTY_METADATA_EXCHANGE_TYPE,
                    WS_HEAP_NEW(heap, WS_METADATA_EXCHANGE_TYPE, (WS_METADATA_EXCHANGE_TYPE_MEX)),
                    sizeof WS_METADATA_EXCHANGE_TYPE
                };

                // URL suffix to get metadata:
                endpointProps[idxProp++] = WS_SERVICE_ENDPOINT_PROPERTY{
                    WS_SERVICE_ENDPOINT_PROPERTY_METADATA_EXCHANGE_URL_SUFFIX,
                    WS_HEAP_NEW(heap, WS_STRING, (ToWsString("mex", heap))),
                    sizeof WS_STRING
                };
            }
            else
                endpointProps = heap.Alloc<WS_SERVICE_ENDPOINT_PROPERTY>(endptPropCount);

            // Properties regarding concurrency:

            endpointProps[idxProp++] = WS_SERVICE_ENDPOINT_PROPERTY {
                WS_SERVICE_ENDPOINT_PROPERTY_MAX_CONCURRENCY,
                WS_HEAP_NEW(heap, decltype(config.maxConcurrency), (config.maxConcurrency)),
                sizeof config.maxConcurrency
            };

            endpointProps[idxProp++] = WS_SERVICE_ENDPOINT_PROPERTY {
                WS_SERVICE_ENDPOINT_PROPERTY_MAX_ACCEPTING_CHANNELS,
                WS_HEAP_NEW(heap, decltype(config.maxAcceptingChannels), (config.maxAcceptingChannels)),
                sizeof config.maxAcceptingChannels
            };

            // Check if count of properties is correct so far
            _ASSERTE(idxProp + 1 == endptPropCount);

            endpoints.reserve(endptsInfo.size());
            WSError err;
            WS_SERVICE_ENDPOINT_PROPERTY *copyOfEndptProps = endpointProps;

            // Now finally create the endpoints:
            for (auto &epnfo : endptsInfo)
            {
                // Use the endpoint properties we have as baseline...
                if (copyOfEndptProps == nullptr)
                {
                    copyOfEndptProps = heap.Alloc<WS_SERVICE_ENDPOINT_PROPERTY>(endptPropCount);
                    memcpy(copyOfEndptProps, endpointProps, sizeof(WS_SERVICE_ENDPOINT_PROPERTY) * endptPropCount);
                }

                // ...complete it with metadata info:
                auto port = heap.Alloc<WS_SERVICE_ENDPOINT_METADATA>();
                port->portName = ToWsXmlString(epnfo.portName, heap);
                port->bindingName = ToWsXmlString(epnfo.bindingName, heap);
                port->bindingNs = ToWsXmlString(epnfo.bindingNs, heap);

                // ...and set this metadata info as the last endpoint property:
                copyOfEndptProps[endptPropCount - 1] =
                    WS_SERVICE_ENDPOINT_PROPERTY {
                        WS_SERVICE_ENDPOINT_PROPERTY_METADATA,
                        port,
                        sizeof *port
                    };

                /* Invoke the callback, which takes care of the remaining settings
                and finally creates the endpoint object: */
                WS_SERVICE_ENDPOINT *endpoint =
                    epnfo.implementations->CreateWSEndpoint(
                        epnfo.address,
                        copyOfEndptProps,
                        endptPropCount,
                        authzCallback,
                        heap,
                        err
                    );

                endpoints.push_back(endpoint);
                copyOfEndptProps = nullptr;
            }
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions regarding errors known to have been previously handled
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when creating service host endpoints: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Sets up the web service given configurations and the WSD file.
    /// </summary>
    /// <param name="wsdFilePath">The web service definition file path.</param>
    /// <param name="config">The configuration common for all service endpoints.</param>
    /// <param name="bindings">The bindings for all service endpoints.</param>
    /// <param name="authzCallback">The callback for authorization.</param>
    /// <param name="enableMEX">If set to <c>true</c>, the endpoint will serve metadata
    /// in the endpoints using WS-MetadataExchange requests.</param>
    void WebServiceHostImpl::Setup(
        const string &wsdFilePath,
        const SvcEndpointsConfig &config,
        const ServiceBindings &bindings,
        WS_SERVICE_SECURITY_CALLBACK authzCallback,
        bool enableMEX)
    {
        CALL_STACK_TRACE;

        try
        {
            _ASSERTE(m_wsSvcHostHandle == nullptr); // cannot setup a service that has been already set

            // First acquire lock to manage service host
            std::lock_guard<std::mutex> lock(m_hostStateMutex);

            rapidxml::xml_document<char> wsdXmlDoc;
            const auto wsdRootNode =
                xml::ParseXmlFromFile(wsdFilePath,
                                      m_wsdContentBuffer,
                                      wsdXmlDoc,
                                      "wsdl:definitions");

            if (wsdRootNode == nullptr)
            {
                throw AppException<std::runtime_error>("Web service definition is not compliant: "
                                                       "element 'wsdl:definitions' is missing!",
                                                       wsdFilePath);
            }

            ParseEndpointsFromWSD(wsdRootNode,
                                  bindings,
                                  m_wsdTargetNs,
                                  m_serviceName,
                                  m_endpointsInfo);

            /* The content of the loaded WSDL file is kept to serve
               metadata requests. If MEX is disabled, get rid of it: */
            if (!enableMEX)
            {
                m_wsdContentBuffer.clear();
                m_wsdContentBuffer.shrink_to_fit();
            }

            // Create the web service endpoints:
            std::vector<const WS_SERVICE_ENDPOINT *> endpoints;
            CreateWebSvcEndpoints(
                config,
                m_endpointsInfo,
                authzCallback,
                enableMEX,
                endpoints,
                m_svcHeap
            );

            // Define a document to be provided by MEX:
            auto documents = m_svcHeap.Alloc<WS_SERVICE_METADATA_DOCUMENT *>(1);
            documents[0] = m_svcHeap.Alloc<WS_SERVICE_METADATA_DOCUMENT>();
            documents[0]->name = m_svcHeap.Alloc<WS_STRING>();
            *documents[0]->name = ToWsString("wsdl", m_svcHeap);
            documents[0]->content = WS_HEAP_NEW(m_svcHeap, WS_XML_STRING, { 0 });
            documents[0]->content->length = m_wsdContentBuffer.size();
            documents[0]->content->bytes = (BYTE *)m_wsdContentBuffer.data();

            // Set service host metadata:
            auto metadata = m_svcHeap.Alloc<WS_SERVICE_METADATA>();
            metadata->documentCount = 1;
            metadata->documents = documents;
            metadata->serviceName = ToWsXmlString(m_serviceName, m_svcHeap);
            metadata->serviceNs = ToWsXmlString(m_wsdTargetNs, m_svcHeap);
                    
            // Service host properties:
            size_t svcPropCount(3);
            auto serviceProps = m_svcHeap.Alloc<WS_SERVICE_PROPERTY>(svcPropCount);

            size_t idxProp(0);
            serviceProps[idxProp++] = WS_SERVICE_PROPERTY {
                WS_SERVICE_PROPERTY_METADATA,
                metadata,
                sizeof *metadata
            };

            serviceProps[idxProp++] = WS_SERVICE_PROPERTY {
                WS_SERVICE_PROPERTY_CLOSE_TIMEOUT,
                WS_HEAP_NEW(m_svcHeap, decltype(config.timeoutClose), (config.timeoutClose)),
                sizeof config.timeoutClose
            };

            serviceProps[idxProp++] = WS_SERVICE_PROPERTY {
                WS_SERVICE_PROPERTY_FAULT_DISCLOSURE,
#            ifdef _DEBUG
                WS_HEAP_NEW(m_svcHeap, WS_FAULT_DISCLOSURE, (WS_FULL_FAULT_DISCLOSURE)),
#            else
                WS_HEAP_NEW(m_svcHeap, WS_FAULT_DISCLOSURE, (WS_MINIMAL_FAULT_DISCLOSURE)),
#            endif
                sizeof WS_FAULT_DISCLOSURE
            };

            // Check the count of properties:
            _ASSERTE(idxProp == svcPropCount);

            // Finally create the web service host:
            WSError err;
            auto hr = WsCreateServiceHost(
                endpoints.data(),
                endpoints.size(),
                serviceProps,
                svcPropCount,
                &m_wsSvcHostHandle,
                err.GetHandle()
            );
            err.RaiseExceptionApiError(hr, "WsCreateServiceHost", "Failed to create web service host");
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions regarding errors known to have been previously handled
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System failure when setting up web service host: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when setting up web service host: " << ex.what();
            throw AppException<std::runtime_error>(oss.str(), wsdFilePath);
        }
    }


    /// <summary>
    /// Sets up the web service given configurations and the WSD file.
    /// </summary>
    /// <param name="wsdFilePath">The web service definition file path.</param>
    /// <param name="config">The configuration common for all service endpoints.</param>
    /// <param name="bindings">The bindings for all service endpoints.</param>
    /// <param name="authzCallback">The callback for authorization.</param>
    /// <param name="enableMEX">If set to <c>true</c>, the endpoint will serve metadata
    /// in the endpoints using WS-MetadataExchange requests.</param>
    void WebServiceHost::Setup(
        const string &wsdFilePath,
        const SvcEndpointsConfig &config,
        const ServiceBindings &bindings,
        WS_SERVICE_SECURITY_CALLBACK authzCallback,
        bool enableMEX)
    {
        m_pimpl->Setup(wsdFilePath, config, bindings, authzCallback, enableMEX);
    }


    /// <summary>
    /// Opens the web service host to start receiving requests.
    /// </summary>
    void WebServiceHostImpl::Open()
    {
        CALL_STACK_TRACE;

        try
        {
            _ASSERTE(m_wsSvcHostHandle != nullptr); // service host must be set up before use

            /* First acquire lock before changing state of service host. This lock guarantees
            that after a failure event, the host will not be left in a transitive state such
            as WS_SERVICE_HOST_STATE_OPENING or WS_SERVICE_HOST_STATE_CLOSING. */
            std::lock_guard<std::mutex> lock(m_hostStateMutex);

            WSError err;
            auto hr = WsOpenServiceHost(m_wsSvcHostHandle, nullptr, err.GetHandle());
            err.RaiseExceptionApiError(hr, "WsOpenServiceHost",
                "Failed to open web service host");
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions regarding errors known to have been previously handled
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System failure when opening web service host: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when opening web service host: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Opens the web service host to start receiving requests.
    /// </summary>
    void WebServiceHost::Open()
    {
        m_pimpl->Open();
    }


    /// <summary>
    /// Closes down communication in the service host (but wait sessions to disconnect)
    /// and make it ready for possible restart.
    /// </summary>
    /// <returns>Whether the service host was actively running and the call managed to stop it.</returns>
    bool WebServiceHostImpl::Close()
    {
        CALL_STACK_TRACE;

        try
        {
            _ASSERTE(m_wsSvcHostHandle != nullptr); // service host must be set up before use

            /* First acquire lock before changing state of service host. This lock guarantees
            that after a failure event, the host will not be left in a transitive state such
            as WS_SERVICE_HOST_STATE_OPENING or WS_SERVICE_HOST_STATE_CLOSING. */
            std::lock_guard<std::mutex> lock(m_hostStateMutex);

            bool wasRunning(false);
            WSError err;

            // Ask for the service current state:
            WS_SERVICE_HOST_STATE state;
            auto hr = WsGetServiceHostProperty(
                m_wsSvcHostHandle,
                WS_SERVICE_PROPERTY_HOST_STATE,
                &state,
                sizeof state,
                err.GetHandle()
            );
            err.RaiseExceptionApiError(hr, "WsGetServiceHostProperty",
                "Failed to get state of web service host");

            // only in open or faulted state can a host be closed
            if (state == WS_SERVICE_HOST_STATE_OPEN
                || state == WS_SERVICE_HOST_STATE_FAULTED)
            {
                // Close service host:
                hr = WsCloseServiceHost(m_wsSvcHostHandle, nullptr, err.GetHandle());
                err.RaiseExceptionApiError(hr, "WsCloseServiceHost",
                    "Failed to close web service host");

                wasRunning = true;
            }

            // Reset the service host:
            hr = WsResetServiceHost(m_wsSvcHostHandle, err.GetHandle());
            err.RaiseExceptionApiError(hr, "WsResetServiceHost",
                "Failed to reset web service host");

            return wasRunning;
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions regarding errors known to have been previously handled
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System failure when closing web service host: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when closing web service host: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Closes down communication in the service host (but wait sessions to disconnect)
    /// and make it ready for possible restart.
    /// </summary>
    /// <returns>Whether the service host was actively running and the call managed to stop it.</returns>
    bool WebServiceHost::Close()
    {
        return m_pimpl->Close();
    }


    /// <summary>
    /// Closes down communication with the service host (immediately, drops clients)
    /// and make it ready for possible restart.
    /// </summary>
    /// <returns>Whether the service host was actively running and the call managed to abort it.</returns>
    bool WebServiceHostImpl::Abort()
    {
        CALL_STACK_TRACE;

        try
        {
            _ASSERTE(m_wsSvcHostHandle != nullptr); // service host must be set up before use

            /* First acquire lock before changing state of service host. This lock guarantees 
            that after a failure event, the host will not be left in a transitive state such 
            as WS_SERVICE_HOST_STATE_OPENING or WS_SERVICE_HOST_STATE_CLOSING. */
            std::lock_guard<std::mutex> lock(m_hostStateMutex);

            bool wasRunning(false);
            WSError err;

            // Ask for the service current state:
            WS_SERVICE_HOST_STATE state;
            auto hr = WsGetServiceHostProperty(
                m_wsSvcHostHandle,
                WS_SERVICE_PROPERTY_HOST_STATE,
                &state,
                sizeof state,
                err.GetHandle()
            );
            err.RaiseExceptionApiError(hr, "WsGetServiceHostProperty",
                "Failed to get state of web service host");

            switch (state)
            {
            // only an open host should be aborted
            case WS_SERVICE_HOST_STATE_OPEN:

                // Abort service host. It will lead it to falted state...
                hr = WsAbortServiceHost(m_wsSvcHostHandle, err.GetHandle());
                err.RaiseExceptionApiError(hr, "WsAbortServiceHost",
                    "Failed to abort web service host");

            /* FALLTHROUGH: The host is left in faulted state due to the current
            abort operation, but it can also happen due to an unknown previous failure.
            In this case, close the host here in this separate case: */
            case WS_SERVICE_HOST_STATE_FAULTED:

                // Close service host:
                hr = WsCloseServiceHost(m_wsSvcHostHandle, nullptr, err.GetHandle());
                err.RaiseExceptionApiError(hr, "WsCloseServiceHost",
                    "Failed to close web service host");

                wasRunning = true;
                break;

            default:
                break;
            }

            // Reset the service host:
            hr = WsResetServiceHost(m_wsSvcHostHandle, err.GetHandle());
            err.RaiseExceptionApiError(hr, "WsResetServiceHost",
                "Failed to reset web service host");

            return wasRunning;
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions regarding errors known to have been previously handled
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System failure when aborting web service host: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when aborting web service host: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Closes down communication with the service host (immediately, drops clients)
    /// and make it ready for possible restart.
    /// </summary>
    /// <returns>Whether the service host was actively running and the call managed to abort it.</returns>
    bool WebServiceHost::Abort()
    {
        return m_pimpl->Abort();
    }

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd
