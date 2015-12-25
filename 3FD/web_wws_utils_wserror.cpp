#include "stdafx.h"
#include "web_wws_utils.h"

#include "callstacktracer.h"
#include "logger.h"
#include <sstream>
#include <codecvt>

namespace _3fd
{
	namespace web
	{
		namespace wws
		{
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
					{
						throw AppException<std::runtime_error>(
							"Failed to delayed-initialize resources for rich error information",
							WWAPI::GetHResultLabel(hr)
						);
					}
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
			void WSError::Reset() throw()
			{
				if (m_wsErrorHandle != nullptr)
				{
					auto hr = WsResetError(m_wsErrorHandle);
					if (hr != S_OK)
					{
						Logger::Write(
							"Failed to reset rich error object for reuse",
							WWAPI::GetHResultLabel(hr),
							Logger::PRIO_CRITICAL
						);
					}
				}
			}

			/// <summary>
			/// Creates an exception for a WWS API error.
			/// </summary>
			/// <param name="wsErrorHandle">A handle to the object holding rich error information.</param>
			/// <param name="hres">The returned HRESULT code.</param>
			/// <param name="funcName">Name of the function.</param>
			/// <param name="message">The main error message.</param>
			/// <returns>An exception object assembled from the given information.</returns>
			static
			AppException<std::runtime_error>
			CreateExceptionApiError(
				WS_ERROR *wsErrorHandle,
				HRESULT hres,
				const char *funcName,
				const char *message)
			{
				try
				{
					std::ostringstream oss;
					oss << (funcName != nullptr ? funcName : "Function")
						<< " returned " << WWAPI::GetHResultLabel(hres);

					unsigned long strCount;
					auto hr = WsGetErrorProperty(
						wsErrorHandle,
						WS_ERROR_PROPERTY_STRING_COUNT,
						&strCount,
						sizeof strCount
					);

					if (hr != S_OK)
					{
						oss << " - Another failure prevented retrieval of further information "
							   "(WsGetErrorProperty returned " << WWAPI::GetHResultLabel(hr) << ')';

						return AppException<std::runtime_error>(message, oss.str());
					}

					if (strCount > 0)
						oss << " - More: ";

					std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

					for (unsigned long idx = 0; idx < strCount; ++idx)
					{
						WS_STRING str;
						hr = WsGetErrorString(wsErrorHandle, idx, &str);

						if (hr == S_OK)
							oss << transcoder.to_bytes(str.chars, str.chars + str.length);
						else
							oss << "NOT AVAILABLE (WsGetErrorString returned " << WWAPI::GetHResultLabel(hr) << ')';

						if (idx + 1 < strCount)
							oss << " // ";
					}

					return AppException<std::runtime_error>(message, oss.str());
				}
				catch (std::exception &ex)
				{
					std::ostringstream oss;
					oss << (funcName != nullptr ? funcName : "Function")
						<< " returned " << WWAPI::GetHResultLabel(hres)
						<< " - Another generic failure prevented retrieval of further details: "
						<< ex.what();

					return AppException<std::runtime_error>(message, oss.str());
				}
			}

			/// <summary>
			/// Check the result code from a WWS API call.
			/// When the given result code means "NOT OKAY", raises an exception
			/// containing rich error information.
			/// </summary>
			/// <param name="hres">The returned HRESULT code.</param>
			/// <param name="funcName">Name of the function.</param>
			/// <param name="message">The error message.</param>
			void WSError::RaiseExceptionApiError(
				HRESULT hres,
				const char *funcName,
				const char *message)
			{
				if (hres == S_OK || hres == WS_S_ASYNC)
					return;

				// Incorrect use of WWS API must trigger an assertion instead of exception:
				//_ASSERTE(hres != E_INVALIDARG && hres != E_HANDLE && hres != E_POINTER);

				Initialize();
				auto ex = CreateExceptionApiError(m_wsErrorHandle, hres, funcName, message);
				Reset();
				throw ex;
			}

			/// <summary>
			/// Check the result code from a WWS API call.
			/// When the given result code means "NOT OKAY", log the event
			/// containing rich error information.
			/// </summary>
			/// <param name="hres">The returned HRESULT code.</param>
			/// <param name="funcName">Name of the function.</param>
			/// <param name="message">The error message.</param>
			void WSError::LogApiError(
				HRESULT hres,
				const char *funcName,
				const char *message) throw()
			{
				if (hres == S_OK || hres == WS_S_ASYNC)
					return;

				// Incorrect use of WWS API must trigger an assertion instead of exception:
				_ASSERTE(hres != E_INVALIDARG && hres != E_HANDLE && hres != E_POINTER);

				Initialize();
				auto ex = CreateExceptionApiError(m_wsErrorHandle, hres, funcName, message);
				Reset();
				Logger::Write(ex, Logger::PRIO_ERROR);
			}

			/// <summary>
			/// Creates an exception from a SOAP fault.
			/// </summary>
			/// <param name="wsErrorHandle">A handle to the object holding rich error information.</param>
			/// <param name="hres">The operation result code.</param>
			/// <param name="message">The base message for the error.</param>
			/// <param name="heap">A heap for memory allocation.</param>
			static
			AppException<std::runtime_error>
			CreateExceptionSoapFault(
				WS_ERROR *wsErrorHandle,
				HRESULT hres,
				const char *message,
				WSHeap &heap)
			{
				try
				{
					WS_FAULT *fault;
					auto hr = WsGetFaultErrorProperty(
						wsErrorHandle,
						WS_FAULT_ERROR_PROPERTY_FAULT,
						&fault,
						sizeof fault
					);

					std::ostringstream oss;

					if (hr != S_OK)
					{
						oss << "Another failure prevented retrieval of information from SOAP fault response "
							   "(WsGetFaultErrorProperty returned " << WWAPI::GetHResultLabel(hr) << ')';

						return AppException<std::runtime_error>(message, oss.str());
					}

					/////////////////
					// FAULT CODE

					string faultCode;

					if (fault->code == nullptr
						|| fault->code->value.localName.bytes == nullptr
						|| fault->code->value.localName.length == 0)
					{
						oss << "Could not retrieve further information because the SOAP fault response was ill-formed: "
							   "fault code was missing";

						return AppException<std::runtime_error>(message, oss.str());
					}

					faultCode = string(
						fault->code->value.localName.bytes,
						fault->code->value.localName.bytes + fault->code->value.localName.length
					);

					std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

					/////////////////
					// FAULT ACTOR

					string faultActor;
					if (fault->actor.chars != nullptr
						&& fault->actor.length != 0)
					{
						faultActor = transcoder.to_bytes(
							fault->actor.chars,
							fault->actor.chars + fault->actor.length
						);
					}

					//////////////////
					// FAULT REASON

					if (fault->reasons[0].text.chars == nullptr
						|| fault->reasons[0].text.length == 0)
					{
						oss << "Could not retrieve further information because the SOAP fault response was ill-formed: "
							   "fault reason was missing";

						return AppException<std::runtime_error>(message, oss.str());
					}

					auto faultReason = transcoder.to_bytes(
						fault->reasons[0].text.chars,
						fault->reasons[0].text.chars + fault->reasons[0].text.length
					);

					// Assemble the main message:

					if (faultCode == "Receiver" || faultCode == "Server")
						oss << "SERVER ERROR";
					else if (faultCode == "Sender" || faultCode == "Client")
						oss << "CLIENT ERROR";
					else
						oss << "(unknown fault code)";

					if (!faultActor.empty())
						oss << " [@ " << faultActor << "]";

					oss << " : " << faultReason;

					////////////////////
					// FAULT DETAILS

					WS_XML_BUFFER *faultDetailsBuffer;
					auto faultDetailDesc = GetFaultDetailDescription(heap);

					hr = WsGetFaultErrorDetail(
						wsErrorHandle,
						&faultDetailDesc,
						WS_READ_REQUIRED_POINTER,
						heap.GetHandle(),
						&faultDetailsBuffer,
						sizeof faultDetailsBuffer
					);

					if (hr != S_OK)
					{
						auto what = oss.str();
						oss.str("");
						oss << "Another failure prevented retrieval of details from SOAP fault response "
							   "(WsGetFaultErrorDetail returned " << WWAPI::GetHResultLabel(hr) << ')';

						return AppException<std::runtime_error>(what, oss.str());
					}

					// Parse details from XML buffer:
					WSXmlReader xmlReader(faultDetailsBuffer);

					xmlReader.ReadStartElement(
						faultDetailDescElemNamespace,
						faultDetailDescElemLocalName
					);

					std::vector<char> utf8textFaultDetails;
					xmlReader.ReadText(utf8textFaultDetails);
					xmlReader.ReadEndElement();

					// Finally return the built exception:
					return AppException<std::runtime_error>(
						oss.str(),
						string(utf8textFaultDetails.begin(), utf8textFaultDetails.end())
					);
				}
				catch (IAppException &ex)
				{
					return AppException<std::runtime_error>(
						message,
						"Secondary failure prevented retrieval of further information from SOAP fault response",
						ex
					);
				}
				catch (std::exception &ex)
				{
					std::ostringstream oss;
					oss << "Another generic failure prevented retrieval of further information "
						   "from SOAP fault response: " << ex.what();

					return AppException<std::runtime_error>(message, oss.str());
				}
			}

			/// <summary>
			/// Check the result code from a proxy operation.
			/// When the given result code means "NOT OKAY", populate the object with rich information
			/// regarding the error, than raises an exception with such content.
			/// </summary>
			/// <param name="hres">The operation result code.</param>
			/// <param name="message">The base message for the error.</param>
			/// <param name="heap">A heap for memory allocation.</param>
			void WSError::RaiseExClientNotOK(HRESULT hres, const char *message, WSHeap &heap)
			{
				if (hres == S_OK || hres == WS_S_ASYNC)
					return;

				// Incorrect use of WWS API must trigger an assertion instead of exception:
				_ASSERTE(hres != E_INVALIDARG && hres != E_HANDLE && hres != E_POINTER);

				Initialize();

				if (hres == WS_E_ENDPOINT_FAULT_RECEIVED)
				{
					// Deserialize content from the SOAP fault response and make an exception from it:
					auto ex = CreateExceptionSoapFault(m_wsErrorHandle, hres, message, heap);
					Reset();
					throw ex;
				}
				else
				{
					auto ex = CreateExceptionApiError(m_wsErrorHandle, hres, nullptr, message);
					Reset();
					throw ex;
				}
			}

			///////////////
			// Helper
			///////////////

			const WS_XML_STRING faultDetailDescElemNamespace = WS_XML_STRING_VALUE("http://3fd.codeplex.com/");

			const WS_XML_STRING faultDetailDescElemLocalName = WS_XML_STRING_VALUE("more");

			/// <summary>
			/// Provides a structure describing what comes inside the
			/// detail element of the SOAP FAULT responses.
			/// </summary>
			/// <param name="heap">A heap from which dynamic memory will be allocated.</param>
			/// <returns>
			/// An instance of <see cref="WS_FAULT_DETAIL_DESCRIPTION" /> initialized
			/// with the values used in the SOAP faults prepared by this library.
			/// </returns>
			WS_FAULT_DETAIL_DESCRIPTION GetFaultDetailDescription(WSHeap &heap)
			{
				WS_FAULT_DETAIL_DESCRIPTION faultDetailDescription{ 0 };

				// the fault detail element has a child described by the structure below:
				faultDetailDescription.detailElementDescription =
					new (heap.Alloc<WS_ELEMENT_DESCRIPTION>()) WS_ELEMENT_DESCRIPTION
					{
						WS_HEAP_NEW(heap, WS_XML_STRING, (faultDetailDescElemLocalName)),
						WS_HEAP_NEW(heap, WS_XML_STRING, (faultDetailDescElemNamespace)),
						WS_XML_BUFFER_TYPE, // value type
						nullptr // no further type description
					};

				return faultDetailDescription;
			}

		}// end of namespace wws
	}// end of namespace web
}// end of namespace _3fd
