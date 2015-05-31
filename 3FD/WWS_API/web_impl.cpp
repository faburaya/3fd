#include "stdafx.h"
#include "web_impl.h"

#include "callstacktracer.h"
#include "logger.h"
#include "Poco\AutoPtr.h"
#include "Poco\DOM\DOMParser.h"
#include "Poco\DOM\Document.h"
#include "Poco\DOM\NodeList.h"
#include "Poco\DOM\NamedNodeMap.h"
#include "Poco\DOM\Attr.h"
#include "Poco\SAX\XMLReader.h"
#include "Poco\SAX\NamespaceSupport.h"
#include <list>
#include <array>
#include <string>
#include <sstream>
#include <fstream>
#include <functional>

namespace _3fd
{
	namespace web
	{
		using std::string;
		using namespace _3fd::core;

		/////////////////////////////
		// WSError Class
		/////////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="WSError"/> class.
		/// In order to save resources, this class uses delayed initialization.
		/// </summary>
		WSError::WSError()
			: m_wsErrorHandle(nullptr) {}

		/// <summary>
		/// Initializes the class resources.
		/// </summary>
		void WSError::Initialize()
		{
			if (m_wsErrorHandle == nullptr)
			{
				auto hr = WsCreateError(nullptr, 0, &m_wsErrorHandle);
				if (hr != S_OK)
					throw AppException<std::runtime_error>("Failed to delayed-initialize object for rich error information", WWAPI::GetHResultLabel(hr));
			}
		}

		/// <summary>
		/// Gets the error handle.
		/// Because the 
		/// </summary>
		/// <returns>The handle for the opaque error object.</returns>
		WS_ERROR * WSError::GetHandle()
		{
			if (m_wsErrorHandle != nullptr)
				return m_wsErrorHandle;
			else
			{
				Initialize();
				return m_wsErrorHandle;
			}
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="WSError"/> class.
		/// </summary>
		WSError::~WSError()
		{
			if (m_wsErrorHandle != nullptr)
				WsFreeError(m_wsErrorHandle);
		}

		/// <summary>
		/// Resets the error object so as to be reused.
		/// </summary>
		void WSError::Reset()
		{
			if (m_wsErrorHandle != nullptr)
			{
				auto hr = WsResetError(m_wsErrorHandle);
				if (hr != S_OK)
					throw AppException<std::runtime_error>("Failed to reset rich error object for reuse", WWAPI::GetHResultLabel(hr));
			}
		}

		/// <summary>
		/// Raises an exception when the error state has been set.
		/// </summary>
		/// <param name="hres">The returned HRESULT code.</param>
		/// <param name="funcName">Name of the function.</param>
		/// <param name="message">The error message.</param>
		/// <param name="svcName">The service name if applicable, otherwise, 'nullptr'.</param>
		void WSError::RaiseExceptionWhenError(
			HRESULT hres,
			const char *funcName,
			const char *message,
			const char *svcName)
		{
			if (hres == S_OK)
				return;
			else
			{
				// Incorrect use of WWS API must trigger an assertion instead of exception:
				//_ASSERTE(hres != E_INVALIDARG && hres != WS_E_INVALID_OPERATION);

				try
				{
					std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
					std::ostringstream oss;
					oss << funcName << " returned " << WWAPI::GetHResultLabel(hres);

					if (svcName != nullptr)
						oss << " (Web service \'" << svcName << "\')";

					Initialize();
					unsigned long strCount;
					auto hr = WsGetErrorProperty(m_wsErrorHandle, WS_ERROR_PROPERTY_STRING_COUNT, &strCount, sizeof strCount);
					if (hr != S_OK)
					{
						oss << "Parallel failure prevented retrieving count of strings from rich error information. WsGetErrorProperty returned " << WWAPI::GetHResultLabel(hr);
						throw AppException<std::runtime_error>(message, oss.str());
					}

					if (strCount > 0)
						oss << " Rich error info: ";

					for (unsigned long idx = 0; idx < strCount; ++idx)
					{
						WS_STRING str;
						hr = WsGetErrorString(m_wsErrorHandle, idx, &str);

						if (hr == S_OK)
							oss << transcoder.to_bytes(str.chars, str.chars + str.length);
						else
							oss << "Failed to get this error string. WsGetErrorString returned " << WWAPI::GetHResultLabel(hr);

						if (idx + 1 < strCount)
							oss << " / ";
					}

					Reset();
					throw AppException<std::runtime_error>(message, oss.str());
				}
				catch (IAppException &ex)
				{
					throw; // just forward exceptions known to have been already handled
				}
				catch (std::exception &ex)
				{
					std::ostringstream oss;
					oss << "Parallel generic failure prevented retrieving the error details. Reason: " << ex.what();
					throw AppException<std::runtime_error>(message, oss.str());
				}
			}
		}

		/////////////////////////////
		// WSHeap Class
		/////////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="WSHeap"/> class.
		/// </summary>
		/// <param name="wsHeapHandle">The heap opaque object handle.</param>
		WSHeap::WSHeap(WS_HEAP *wsHeapHandle)
			: m_wsHeapHandle(wsHeapHandle), m_allowRelease(false) {}

		/// <summary>
		/// Initializes a new instance of the <see cref="WSHeap"/> class.
		/// </summary>
		WSHeap::WSHeap() :
			m_wsHeapHandle(nullptr), 
			m_allowRelease(true) 
		{
			CALL_STACK_TRACE;
			WSError err;
			auto hr = WsCreateHeap(1024, 0, nullptr, 0, &m_wsHeapHandle, err.GetHandle());
			err.RaiseExceptionWhenError(hr, "WsCreateHeap", "Failed to create heap");
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="WSHeap"/> class.
		/// </summary>
		WSHeap::~WSHeap()
		{
			if (m_allowRelease)
				WsFreeHeap(m_wsHeapHandle);
		}

		/// <summary>
		/// Resets this instance.
		/// </summary>
		void WSHeap::Reset()
		{
			CALL_STACK_TRACE;
			WSError err;
			auto hr = WsResetHeap(m_wsHeapHandle, err.GetHandle());
			err.RaiseExceptionWhenError(hr, "WsResetHeap", "Failed to release heap allocations");
		}

		/// <summary>
		/// Allocates some memory.
		/// </summary>
		/// <param name="qtBytes">The amount of bytes to allocate.</param>
		/// <returns>A pointer to the allocated amount of memory.</returns>
		void * WSHeap::Alloc(size_t qtBytes)
		{
			CALL_STACK_TRACE;
			WSError err;
			void *ptr;
			auto hr = WsAlloc(m_wsHeapHandle, qtBytes, &ptr, err.GetHandle());
			err.RaiseExceptionWhenError(hr, "WsAlloc", "Failed to allocate heap memory");
			return ptr;
		}

		/////////////////////////////
		// WebServiceHost Class
		/////////////////////////////

		/// <summary>
		/// Reads a file to a buffer.
		/// </summary>
		/// <param name="path">The file path.</param>
		/// <param name="buffer">The buffer.</param>
		static void ReadFile(const string &path, std::vector<char> &buffer)
		{
			CALL_STACK_TRACE;

			try
			{
				buffer.clear(); // if something goes wrong, do not keep any previous content in the buffer

				std::ifstream inputStream(path, std::ios::in | std::ios::binary); // open file

				if (inputStream.is_open() == false)
				{
					std::ostringstream oss;
					oss << "Failed to open file \'" << path << "\' in read mode";
					throw AppException<std::runtime_error>(oss.str());
				}

				const auto fileSizeBytes = inputStream.seekg(0, std::ios::end).tellg().seekpos(); // move cursor to the end to get the zize

				// File is not trunked:
				if (fileSizeBytes > 0)
				{
					inputStream.seekg(0, std::ios::beg); // rewind

					// Read the file contents to the buffer:
					buffer.resize(fileSizeBytes);
					inputStream.read(buffer.data(), buffer.size());
					_ASSERTE(inputStream.gcount() == buffer.size());

					if (inputStream.bad())
					{
						std::ostringstream oss;
						oss << "Failed to read from file \'" << path << '\'';
						throw AppException<std::runtime_error>(oss.str());
					}

					inputStream.close(); // release the file
				}
			}
			catch (IAppException &ex)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when reading file \'" << path << "\'. Reason: " << ex.what();
				throw AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Parses information about endpoints from a WSDL document.
		/// </summary>
		/// <param name="wsdContent">Web service definition content previously loaded from a file.</param>
		/// <param name="targetNamespace">Where to save the target namespace.</param>
		/// <param name="serviceName">Where to save the service name.</param>
		/// <param name="endpoints">Where to save the parsed information regarding the endpoints.</param>
		static void ParseEndpointsFromWSD(
			const std::vector<char> &wsdContent,
			string &targetNamespace,
			string &serviceName,
			std::vector<SvcEndpointInfo> &endpoints)
		{
			using namespace Poco;

			CALL_STACK_TRACE;
			/* Assumes the usage of HTTP & SOAP, but does not check if the document is thoroughly
			well formed. Because this library is meant to integrate with wsutil.exe generated code,
			the WSDL document is expected to follow the specification in http://www.w3.org/TR/wsdl.
			Also, the bindings MUST BE declared in the target namespace using the prefix tns. */
			try
			{
				// If anything goes wrong, do not keep any previous content in the output:
				targetNamespace.clear();
				serviceName.clear();
				endpoints.clear();

				// Fundamental namespaces URI's:
				static const string wsdlNs("http://schemas.xmlsoap.org/wsdl/"),
					soapNs("http://schemas.xmlsoap.org/wsdl/soap/");

				XML::NamespaceSupport nsmap;
				nsmap.declarePrefix("wsdl", wsdlNs);
				nsmap.declarePrefix("soap", soapNs);

				// Parse XML from memory:
				XML::DOMParser parser;
				parser.setFeature(XML::SAXParser::FEATURE_NAMESPACE_PREFIXES, true);
				parser.setFeature(XML::SAXParser::FEATURE_NAMESPACES, true);
				AutoPtr<XML::Document> document = parser.parseMemory(wsdContent.data(), wsdContent.size());

				// Get /wsdl:definitions
				auto definitions = static_cast<XML::Element *> (document->getNodeByPathNS("/wsdl:definitions", nsmap));
				if (definitions == nullptr)
					throw AppException<std::runtime_error>("Web service definition is not compliant", "The WSDL definitions element is missing");

				// Get /wsdl:definitions[@targetNamespace]
				auto attr = definitions->getAttributeNode("targetNamespace");
				if (attr != nullptr)
				{
					targetNamespace = attr->nodeValue();
					nsmap.declarePrefix("tns", targetNamespace);
				}
				else
					throw AppException<std::runtime_error>("Web service definition is not compliant", "The target namespace is missing from WSDL document");

				// Get /wsdl:definitions/wsdl:service
				auto svcElement = definitions->getChildElementNS(wsdlNs, "service");
				if (svcElement == nullptr)
					throw AppException<std::runtime_error>("Web service definition is not compliant", "The WSDL service element is missing from document");

				// Get /wsdl:definitions/wsdl:service[@name]
				attr = svcElement->getAttributeNode("name");
				if (attr != nullptr)
					serviceName = attr->nodeValue();
				else
					throw AppException<std::runtime_error>("Web service definition is not compliant", "The attribute \'name\' was missing from the WSDL service element");

				// Iterate over each 'wsdl:port' element:
				AutoPtr<XML::NodeList> portNodes = svcElement->getElementsByTagNameNS(wsdlNs, "port");
				for (unsigned long idx = 0; idx < portNodes->length(); ++idx)
				{
					auto portElement = static_cast<XML::Element *> (portNodes->item(idx));

					// Add a new endpoint to the service
					endpoints.emplace_back();
					auto &endpoint = endpoints.back();

					// Get /wsdl:definitions/wsdl:service/wsdl:port[@name]
					attr = portElement->getAttributeNode("name");
					if (attr != nullptr)
						endpoint.portName = attr->nodeValue();
					else
					{
						std::ostringstream oss;
						oss << "Attribute \'name\' is missing from WSDL port element in service \'" << serviceName << '\'';
						throw AppException<std::runtime_error>("Web service definition is not compliant", oss.str());
					}

					// Get /wsdl:definitions/wsdl:service/wsdl:port[@binding]
					attr = portElement->getAttributeNode("binding");
					if (attr == nullptr)
					{
						std::ostringstream oss;
						oss << "Attribute \'binding\' is missing from WSDL port \'" << endpoint.portName
							<< "\' in service \'" << serviceName << '\'';
						throw AppException<std::runtime_error>("Web service definition is not compliant", oss.str());
					}

					if (nsmap.processName(attr->nodeValue(), endpoint.bindingNs, endpoint.bindingName, false) == false)
					{
						std::ostringstream oss;
						oss << "Could not resolve WSDL binding \'" << attr->nodeValue()
							<< "\' of port \'" << endpoint.portName
							<< "\' in service \'" << serviceName << '\'';
						throw AppException<std::runtime_error>("Web service definition is not compliant", oss.str());
					}

					// Get /wsdl:definitions/wsdl:service/wsdl:port/soap:address[@location]
					attr = static_cast<XML::Attr *> (portElement->getNodeByPath("/soap:address[@location]"));
					if (attr != nullptr)
						endpoint.address = attr->nodeValue();
					else
					{
						std::ostringstream oss;
						oss << "Endpoint soap address not found for WSDL port \'" << endpoint.portName
							<< "\' in service \'" << serviceName << '\'';
						throw AppException<std::runtime_error>("Web service definition is not compliant", oss.str());
					}
				}// for loop end

				if (endpoints.empty())
					throw AppException<std::runtime_error>("Web service definition is not compliant", "No valid specification for endpoint has been found");
			}
			catch (IAppException &ex)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (Poco::Exception &ex)
			{
				std::ostringstream oss;
				oss << "POCO C++ library reported: " << ex.message();
				throw AppException<std::runtime_error>("Failed to parse web service definition", oss.str());
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
		/// <param name="fromEndpointsInfo">Contains the information about each endpoint to create.</param>
		/// <param name="functionTable">The function table.</param>
		/// <param name="createWebSvcEndpointCallback">The callback which invokes code generated by wsutil.exe to create a service endpoint.</param>
		/// <param name="toSvcEndpoints">Where to save the endpoints to be created.</param>
		/// <param name="heap">The heap that provides memory allocation.</param>
		static void CreateWebSvcEndpoints(
			const SvcEndpointsConfig &config,
			const std::vector<SvcEndpointInfo> &fromEndpointsInfo,
			void *functionTable,
			CallbackWrapperCreateWSEndpoint createWebSvcEndpointCallback,
			std::vector<const WS_SERVICE_ENDPOINT *> &toSvcEndpoints,
			WSHeap &heap)
		{
			CALL_STACK_TRACE;

			try
			{
				toSvcEndpoints.clear(); // if anything goes wrong, do not keep any previous content in the output
				toSvcEndpoints.reserve(fromEndpointsInfo.size());

				for (auto &endpointInfo : fromEndpointsInfo)
				{
					// The endpoint port to show in the WSDL document (available through MEX):
					auto port = heap.Alloc<WS_SERVICE_ENDPOINT_METADATA>();
					port->portName = new (heap.Alloc(sizeof(WS_XML_STRING))) WS_XML_STRING{ endpointInfo.portName.length(), (unsigned char *)endpointInfo.portName.data(), nullptr, 0 };
					port->bindingName = new (heap.Alloc(sizeof(WS_XML_STRING))) WS_XML_STRING{ endpointInfo.bindingName.length(), (unsigned char *)endpointInfo.bindingName.data(), nullptr, 0 };
					port->bindingNs = new (heap.Alloc(sizeof(WS_XML_STRING))) WS_XML_STRING{ endpointInfo.bindingNs.length(), (unsigned char *)endpointInfo.bindingNs.data(), nullptr, 0 };

					auto mexUrlSuffix = heap.Alloc<WS_STRING>();
					*mexUrlSuffix = WS_STRING_VALUE(L"mex"); // URL suffix to get MEX

					auto metadataExchangeType = heap.Alloc<WS_METADATA_EXCHANGE_TYPE>();
					*metadataExchangeType = WS_METADATA_EXCHANGE_TYPE_MEX; // use MEX

					// Set the endpoint properties from the settings in the parameters:
					auto endpointProps = heap.Alloc<WS_SERVICE_ENDPOINT_PROPERTY>(5);
					endpointProps[0] = WS_SERVICE_ENDPOINT_PROPERTY{ WS_SERVICE_ENDPOINT_PROPERTY_MAX_ACCEPTING_CHANNELS, const_cast<unsigned int *> (&config.maxAcceptingChannels), sizeof config.maxAcceptingChannels };
					endpointProps[1] = WS_SERVICE_ENDPOINT_PROPERTY{ WS_SERVICE_ENDPOINT_PROPERTY_MAX_CONCURRENCY, const_cast<unsigned int *> (&config.maxConcurrency), sizeof config.maxConcurrency };
					endpointProps[2] = WS_SERVICE_ENDPOINT_PROPERTY{ WS_SERVICE_ENDPOINT_PROPERTY_METADATA_EXCHANGE_TYPE, metadataExchangeType, sizeof *metadataExchangeType };
					endpointProps[3] = WS_SERVICE_ENDPOINT_PROPERTY{ WS_SERVICE_ENDPOINT_PROPERTY_METADATA_EXCHANGE_URL_SUFFIX, mexUrlSuffix, sizeof *mexUrlSuffix };
					endpointProps[4] = WS_SERVICE_ENDPOINT_PROPERTY{ WS_SERVICE_ENDPOINT_PROPERTY_METADATA, port, sizeof *port };

					// Invoke the callback, which takes care of the remaining settings:
					WSError err;
					WS_SERVICE_ENDPOINT *endpoint;
					auto hr = createWebSvcEndpointCallback(
						endpointInfo.address,
						functionTable,
						nullptr,
						endpointProps,
						5,
						heap,
						&endpoint
						);

					if (hr != S_OK)
					{
						std::ostringstream oss;
						oss << "Failed to create web service endpoint at " << endpointInfo.address;
						err.RaiseExceptionWhenError(hr, "WsCreateServiceEndpointFromTemplate", oss.str().c_str());
					}

					// Add the newly created endpoint to the output vector
					toSvcEndpoints.push_back(endpoint);
				}
			}
			catch (IAppException &ex)
			{
				throw; // just forward exceptions known to have been already handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when creating service endpoint: " << ex.what();
				throw AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="WebServiceHostImpl" /> class.
		/// </summary>
		WebServiceHostImpl::WebServiceHostImpl()
			try :
			m_webSvcHostHandle(nullptr),
			m_svcThread(),
			m_svcStateMutex(),
			m_svcHeap()
		{
		}
		catch (IAppException &ex)
		{
			throw; // just forward exceptions known to have been already handled
		}
		catch (std::system_error &ex)
		{
			CALL_STACK_TRACE;
			std::ostringstream oss;
			oss << "Failed to create web service host: " << StdLibExt::GetDetailsFromSystemError(ex);
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
		/// Finalizes an instance of the <see cref="WebServiceHostImpl"/> class.
		/// </summary>
		WebServiceHostImpl::~WebServiceHostImpl()
		{
			if (m_webSvcHostHandle == nullptr)
				return;

			CALL_STACK_TRACE;

			try
			{
				// If the service thread is running:
				if (m_svcThread.get_id() != std::thread::id())
				{
					// Ask for the service current state:
					WSError err;
					WS_SERVICE_HOST_STATE state;
					auto hr = WsGetServiceHostProperty(
						m_webSvcHostHandle,
						WS_SERVICE_PROPERTY_HOST_STATE,
						&state,
						sizeof state,
						err.GetHandle()
						);
					err.RaiseExceptionWhenError(hr, "WsGetServiceHostProperty", "Failed to get state of web service host", m_serviceName);

					if (state == WS_SERVICE_HOST_STATE_OPENING
						|| state == WS_SERVICE_HOST_STATE_OPEN)
					{
						std::ostringstream oss;
						oss << "Stopping web service \'" << m_serviceName << "\'...";
						Logger::GetInstance().Write(oss.str(), Poco::Message::Priority::PRIO_INFORMATION);

						// Abort web service asynchronous execution
						hr = WsAbortServiceHost(m_webSvcHostHandle, err.GetHandle());
						err.RaiseExceptionWhenError(hr, "WsAbortServiceHost", "Failed to abort web service asynchronous execution", m_serviceName);

						// Close web service:
						hr = WsCloseServiceHost(m_webSvcHostHandle, nullptr, err.GetHandle());
						err.RaiseExceptionWhenError(hr, "WsCloseServiceHost", "Failed to close web service host", m_serviceName);

						oss.str("");
						oss << "Web service \'" << m_serviceName << "\' successfully stopped";
						Logger::GetInstance().Write(oss.str(), Poco::Message::Priority::PRIO_INFORMATION);
					}

					// Ensure thread termination:
					if (m_svcThread.joinable())
						m_svcThread.join();
				}

				// Finalize and release resources
				WsFreeServiceHost(m_webSvcHostHandle);
			}
			catch (IAppException &ex)
			{
				Logger::GetInstance().Write(ex, Poco::Message::Priority::PRIO_CRITICAL);
			}
			catch (std::system_error &ex)
			{
				std::ostringstream oss;
				oss << "Failed to terminate web service host asynchronous execution: " << StdLibExt::GetDetailsFromSystemError(ex);
				Logger::GetInstance().Write(oss.str(), Poco::Message::Priority::PRIO_CRITICAL);
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when terminating web service host: " << ex.what();
				Logger::GetInstance().Write(oss.str(), Poco::Message::Priority::PRIO_CRITICAL);
			}
		}

		/// <summary>
		/// Setups the specified WSD file path.
		/// </summary>
		/// <param name="wsdFilePath">The web service definition file path.</param>
		/// <param name="config">The configuration common for all service endpoints.</param>
		/// <param name="createWebSvcEndpointCallback">The callback implementation for endpoint creation (generated by wsutil.exe).</param>
		/// <param name="functionTable">The function table, which contains the pointers for the functions implementing the service operations.</param>
		void WebServiceHostImpl::Setup(
			const string &wsdFilePath,
			const SvcEndpointsConfig &config,
			CallbackWrapperCreateWSEndpoint createWebSvcEndpointCallback,
			void *functionTable)
		{
			CALL_STACK_TRACE;

			try
			{
				_ASSERTE(m_webSvcHostHandle == nullptr); // cannot setup a service that has been already set

				// First acquire lock to manage service host
				std::lock_guard<std::mutex> lock(m_svcStateMutex);

				// Read web service definition from file
				ReadFile(wsdFilePath, m_wsdContentBuffer);

				// Parse information from the WSDL:
				ParseEndpointsFromWSD(m_wsdContentBuffer, m_wsdTargetNs, m_serviceName, m_endpointsInfo);

				// Create the web service endpoints:
				std::vector<const WS_SERVICE_ENDPOINT *> endpoints;
				CreateWebSvcEndpoints(config, m_endpointsInfo, functionTable, createWebSvcEndpointCallback, endpoints, m_svcHeap);

				// Define a document to be provided by MEX:
				auto document = m_svcHeap.Alloc<WS_SERVICE_METADATA_DOCUMENT>();
				document->name = nullptr;
				document->content = new (m_svcHeap.Alloc(sizeof(WS_XML_STRING))) WS_XML_STRING{ 0 };
				document->content->length = m_wsdContentBuffer.size();
				document->content->bytes = reinterpret_cast<unsigned char *> (m_wsdContentBuffer.data());

				// Set service host metadata:
				WS_SERVICE_METADATA metadata;
				metadata.documentCount = 1;
				metadata.documents = &document;
				metadata.serviceName = new (m_svcHeap.Alloc(sizeof(WS_XML_STRING))) WS_XML_STRING{ m_serviceName.length(), (unsigned char *)m_serviceName.data(), nullptr, 0 };
				metadata.serviceNs = new (m_svcHeap.Alloc(sizeof(WS_XML_STRING))) WS_XML_STRING{ m_wsdTargetNs.length(), (unsigned char *)m_wsdTargetNs.data(), nullptr, 0 };

#			ifdef _DEBUG
				WS_FAULT_DISCLOSURE faultDisclosure(WS_FULL_FAULT_DISCLOSURE);
#			else
				WS_FAULT_DISCLOSURE faultDisclosure(WS_MINIMAL_FAULT_DISCLOSURE);
#			endif
				// Service host properties:
				std::array<WS_SERVICE_PROPERTY, 2> serviceProps =
				{
					WS_SERVICE_PROPERTY{ WS_SERVICE_PROPERTY_METADATA, &metadata, sizeof metadata },
					WS_SERVICE_PROPERTY{ WS_SERVICE_PROPERTY_FAULT_DISCLOSURE, &faultDisclosure, sizeof faultDisclosure }
				};

				// Finally create the web service host:
				WSError err;
				auto hr = WsCreateServiceHost(
					endpoints.data(),
					endpoints.size(),
					serviceProps.data(),
					serviceProps.size(),
					&m_webSvcHostHandle,
					err.GetHandle()
					);
				err.RaiseExceptionWhenError(hr, "WsCreateServiceHost", "Failed to create web service host", m_serviceName);
			}
			catch (IAppException &ex)
			{
				throw; // just forward exceptions known to have been already handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when setting up web service host: " << ex.what();
				throw AppException<std::runtime_error>(oss.str(), wsdFilePath);
			}
		}

		/// <summary>
		/// Opens the web service host to start receiving requests.
		/// </summary>
		void WebServiceHostImpl::OpenAsync()
		{
			CALL_STACK_TRACE;

			try
			{
				_ASSERTE(m_webSvcHostHandle != nullptr); // service host must be set up before use

				// First acquire lock to manage service host
				std::lock_guard<std::mutex> lock(m_svcStateMutex);

				if (m_svcThread.get_id() != std::thread::id())
				{
					std::ostringstream oss;
					oss << "Tried to open web service \'" << m_serviceName << "\' when it was already running asynchronously";
					throw AppException<std::runtime_error>(oss.str());
				}

				std::ostringstream oss;
				oss << "Starting web service \'" << m_serviceName << '\'';
				Logger::GetInstance().Write(oss.str(), Poco::Message::Priority::PRIO_INFORMATION);

				// Open the web service host for client requests in a parallel thread:
				m_svcThread.swap(
					std::thread([this]()
				{
					CALL_STACK_TRACE;

					try
					{
						WSError err;
						auto hr = WsOpenServiceHost(m_webSvcHostHandle, nullptr, err.GetHandle());
						err.RaiseExceptionWhenError(hr, "WsOpenServiceHost", "Failed to open web service host", m_serviceName);
					}
					catch (IAppException &ex)
					{
						Logger::GetInstance().Write(ex, Poco::Message::Priority::PRIO_CRITICAL);
					}
				})
					);
			}
			catch (IAppException &ex)
			{
				throw; // just forward exceptions known to have been already handled
			}
			catch (std::system_error &ex)
			{
				std::ostringstream oss;
				oss << "Failed to open web service host asynchronously: " << StdLibExt::GetDetailsFromSystemError(ex);
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
		/// Closes down communication with the service host (but wait sessions to disconnect) and make it ready for possible restart.
		/// </summary>
		/// <returns>Whether the call managed to close the service host when it was actively running.</returns>
		bool WebServiceHostImpl::CloseAsync()
		{
			CALL_STACK_TRACE;

			try
			{
				_ASSERTE(m_webSvcHostHandle != nullptr); // service host must be set up before use

				std::lock_guard<std::mutex> lock(m_svcStateMutex);

				bool wasRunning(false);
				WSError err;
				HRESULT hr;

				// The web service thread is still running:
				if (m_svcThread.get_id() != std::thread::id())
				{
					// Ask for the service current state:
					WS_SERVICE_HOST_STATE state;
					auto hr = WsGetServiceHostProperty(
						m_webSvcHostHandle,
						WS_SERVICE_PROPERTY_HOST_STATE,
						&state,
						sizeof state,
						err.GetHandle()
						);
					err.RaiseExceptionWhenError(hr, "WsGetServiceHostProperty", "Failed to get state of web service host", m_serviceName);

					if (state == WS_SERVICE_HOST_STATE_OPENING
						|| state == WS_SERVICE_HOST_STATE_OPEN)
					{
						std::ostringstream oss;
						oss << "Stopping web service \'" << m_serviceName << "\'...";
						Logger::GetInstance().Write(oss.str(), Poco::Message::Priority::PRIO_INFORMATION);

						// Close service host:
						hr = WsCloseServiceHost(m_webSvcHostHandle, nullptr, err.GetHandle());
						err.RaiseExceptionWhenError(hr, "WsCloseServiceHost", "Failed to close web service host", m_serviceName);
						wasRunning = true;

						oss.str("");
						oss << "Web service \'" << m_serviceName << "\' successfully stopped";
						Logger::GetInstance().Write(oss.str(), Poco::Message::Priority::PRIO_INFORMATION);
					}

					// Wait for thread termination:
					if (m_svcThread.joinable())
						m_svcThread.join();
				}

				// Reset the service host:
				hr = WsResetServiceHost(m_webSvcHostHandle, err.GetHandle());
				err.RaiseExceptionWhenError(hr, "WsResetServiceHost", "Failed to reset web service host", m_serviceName);

				return wasRunning;
			}
			catch (IAppException &ex)
			{
				throw; // just forward exceptions known to have been already handled
			}
			catch (std::system_error &ex)
			{
				std::ostringstream oss;
				oss << "Failed to close web service host: " << StdLibExt::GetDetailsFromSystemError(ex);
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
		/// Closes down communication with the service host (immediately, drops clients) and make it ready for possible restart.
		/// </summary>
		/// <returns>Whether the call managed to abort the service host when it was actively running.</returns>
		bool WebServiceHostImpl::AbortAsync()
		{
			CALL_STACK_TRACE;

			try
			{
				_ASSERTE(m_webSvcHostHandle != nullptr); // service host must be set up before use

				// First acquire lock to manage service host
				std::lock_guard<std::mutex> lock(m_svcStateMutex);

				bool wasRunning(false);
				WSError err;
				HRESULT hr;

				// The web service thread is sill running:
				if (m_svcThread.get_id() != std::thread::id())
				{
					// Ask for the service current state:
					WS_SERVICE_HOST_STATE state;
					auto hr = WsGetServiceHostProperty(
						m_webSvcHostHandle,
						WS_SERVICE_PROPERTY_HOST_STATE,
						&state,
						sizeof state,
						err.GetHandle()
						);
					err.RaiseExceptionWhenError(hr, "WsGetServiceHostProperty", "Failed to get state of web service host", m_serviceName);

					if (state == WS_SERVICE_HOST_STATE_OPENING
						|| state == WS_SERVICE_HOST_STATE_OPEN)
					{
						std::ostringstream oss;
						oss << "Stopping web service \'" << m_serviceName << "\'...";
						Logger::GetInstance().Write(oss.str(), Poco::Message::Priority::PRIO_INFORMATION);

						// Abort service host:
						hr = WsAbortServiceHost(m_webSvcHostHandle, err.GetHandle());
						err.RaiseExceptionWhenError(hr, "WsAbortServiceHost", "Failed to abort web service host", m_serviceName);

						// Close service host:
						hr = WsCloseServiceHost(m_webSvcHostHandle, nullptr, err.GetHandle());
						err.RaiseExceptionWhenError(hr, "WsCloseServiceHost", "Failed to close web service host", m_serviceName);
						wasRunning = true;

						oss.str("");
						oss << "Web service \'" << m_serviceName << "\' successfully stopped";
						Logger::GetInstance().Write(oss.str(), Poco::Message::Priority::PRIO_INFORMATION);
					}

					// Ensure thread termination:
					if (m_svcThread.joinable())
						m_svcThread.join();
				}

				// Reset the service host:
				hr = WsResetServiceHost(m_webSvcHostHandle, err.GetHandle());
				err.RaiseExceptionWhenError(hr, "WsResetServiceHost", "Failed to reset web service host", m_serviceName);

				return wasRunning;
			}
			catch (IAppException &ex)
			{
				throw; // just forward exceptions known to have been already handled
			}
			catch (std::system_error &ex)
			{
				std::ostringstream oss;
				oss << "Failed to abort web service host: " << StdLibExt::GetDetailsFromSystemError(ex);
				throw AppException<std::runtime_error>(oss.str());
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when aborting web service host: " << ex.what();
				throw AppException<std::runtime_error>(oss.str());
			}
		}

		/////////////////////
		// Utilities
		/////////////////////

		/// <summary>
		/// Creates a SOAP fault from an exception (server error) and record it into rich error information.
		/// </summary>
		/// <param name="operEx">The exception thrown by the web service operation implementation.</param>
		/// <param name="wsOperContextHandle">Handle for the the current web service operation context.</param>
		/// <param name="wsErrorHandle">Handle for the rich error info utility.</param>
		void CreateSoapFault(core::IAppException &operEx, WS_OPERATION_CONTEXT *wsOperContextHandle, WS_ERROR *wsErrorHandle)
		{
			CALL_STACK_TRACE;

			try
			{
				// Get the heap from the current operation context:
				WSError err;
				WS_HEAP *wsHeapHandle;
				auto hr = WsGetOperationContextProperty(
					wsOperContextHandle,
					WS_OPERATION_CONTEXT_PROPERTY_HEAP,
					&wsHeapHandle,
					sizeof wsHeapHandle,
					err.GetHandle()
				);
				err.RaiseExceptionWhenError(hr, "WsGetOperationContextProperty", "Failed to retrieve heap object from web service operation context");

				WSHeap heap(wsHeapHandle); // wraps the heap handle into an object

				// Create an empty SOAP fault structure
				auto fault = heap.Alloc<WS_FAULT>();
				*fault = {};

				// The fault code:
				fault->code = heap.Alloc<WS_FAULT_CODE>();
				fault->code->subCode = nullptr;
				fault->code->value.ns = WS_XML_STRING_VALUE("http://www.w3.org/2003/05/soap-envelope");
				fault->code->value.localName = WS_XML_STRING_VALUE("Receiver");

				// Go through the nested chain of exceptions and translate their messages to unicode:
				std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
				std::list<std::wstring> messages;
				auto exPtr = &operEx;
				while (exPtr != nullptr)
				{
					messages.push_back(
						transcoder.from_bytes(exPtr->GetErrorMessage())
						);
					exPtr = exPtr->GetInnerException();
				}

				// The fault reasons:
				fault->reasonCount = messages.size();
				fault->reasons = heap.Alloc<WS_FAULT_REASON>(fault->reasonCount);

				// Copy the exceptions messages into the fault reasons:
				int idx(0);
				while (messages.empty() == false)
				{
					auto &msg = messages.front();
					auto &reason = fault->reasons[idx];
					reason.lang = WS_STRING_VALUE(L"en");
					reason.text.length = msg.length();
					reason.text.chars = heap.Alloc<wchar_t>(msg.length());
					memcpy(reason.text.chars, msg.data(), msg.length() * sizeof msg[0]);
					messages.pop_front();
					++idx;
				}

				// Record the assembled SOAP fault into the error object:
				hr = WsSetFaultErrorProperty(wsErrorHandle, WS_FAULT_ERROR_PROPERTY_FAULT, fault, sizeof *fault);
				err.RaiseExceptionWhenError(hr, "", "Failed to record SOAP fault response into rich error information");
			}
			catch (IAppException &ex)
			{
				Logger::GetInstance().Write(operEx, Poco::Message::Priority::PRIO_ERROR);
				Logger::GetInstance().Write(ex, Poco::Message::Priority::PRIO_CRITICAL);
			}
			catch (std::exception &ex)
			{
				Logger::GetInstance().Write(operEx, Poco::Message::Priority::PRIO_ERROR);
				std::ostringstream oss;
				oss << "Generic failure when assembling SOAP fault response: " << ex.what();
				Logger::GetInstance().Write(oss.str(), Poco::Message::Priority::PRIO_CRITICAL);
			}
		}

	}// end of namespace web

}// end of namespace _3fd