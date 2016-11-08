#include "stdafx.h"
#include "web_wws_impl_utils.h"

#include "callstacktracer.h"
#include "logger.h"

#include <codecvt>

namespace _3fd
{
namespace web
{
namespace wws
{
	using namespace _3fd::core;

	/// <summary>
	/// Creates from a std::string a WS_XML_STRING structure.
	/// </summary>
	/// <param name="str">The original std::string object.</param>
	/// <returns>
	/// A WS_XML_STRING structure referencing the data in the original std::string object.
	/// </returns>
	WS_XML_STRING ToWsXmlString(const string &str)
	{
		return WS_XML_STRING
		{
			static_cast<ULONG> (str.length()),
			(BYTE *)str.data(),
			nullptr,
			0
		};
	}

	/// <summary>
	/// Creates from a std::string a heap allocated WS_XML_STRING structure.
	/// </summary>
	/// <param name="str">The original std::string object.</param>
	/// <param name="heap">The heap to allocate memory from.</param>
	/// <returns>
	/// A pointer to a heap allocated WS_XML_STRING structure referencing
	/// the data in the original std::string object.
	/// </returns>
	WS_XML_STRING *ToWsXmlString(const string &str, WSHeap &heap)
	{
		void *ptr = heap.Alloc<WS_XML_STRING>();
		return new (ptr) WS_XML_STRING
		{
			static_cast<ULONG> (str.length()),
			(BYTE *)str.data(),
			nullptr,
			0
		};
	}

	/// <summary>
	/// Creates from a std::string a heap allocated WS_STRING structure.
	/// </summary>
	/// <param name="str">The original std::string object.</param>
	/// <param name="heap">The heap to allocate memory from.</param>
	/// <returns>
	/// A pointer to a heap allocated WS_STRING structure referencing
	/// the data in the original std::string object.
	/// </returns>
	WS_STRING ToWsString(const string &str, WSHeap &heap)
	{
		WS_STRING obj;
		obj.length = mbstowcs(nullptr, str.data(), str.length());
		obj.chars = heap.Alloc<wchar_t>(obj.length);
		mbstowcs(obj.chars, str.data(), str.length());
		return obj;
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
	/// Initializes a new instance of the <see cref="WSHeap" /> class.
	/// </summary>
	/// <param name="nBytes">The amount of bytes to allocate.</param>
	WSHeap::WSHeap(size_t nBytes) :
		m_wsHeapHandle(nullptr),
		m_allowRelease(true)
	{
		CALL_STACK_TRACE;
		WSError err;
		auto hr = WsCreateHeap(nBytes, 0, nullptr, 0, &m_wsHeapHandle, err.GetHandle());
		err.RaiseExceptionApiError(hr, "WsCreateHeap", "Failed to create heap");
	}

	/// <summary>
	/// Finalizes an instance of the <see cref="WSHeap"/> class.
	/// </summary>
	WSHeap::~WSHeap()
	{
		if (m_allowRelease && m_wsHeapHandle != nullptr)
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
		err.RaiseExceptionApiError(hr, "WsResetHeap", "Failed to release heap allocations");
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
		err.RaiseExceptionApiError(hr, "WsAlloc", "Failed to allocate heap memory");
		return ptr;
	}

    /////////////////////
    // Host Utilities
    /////////////////////

    /// <summary>
    /// Creates a SOAP fault response from an exception (service error)
    /// and record it as rich error information.
    /// </summary>
    /// <param name="reason">The fault reason.</param>
    /// <param name="details">The fault details.</param>
    /// <param name="action">The SOAP action to which the fault refers.</param>
    /// <param name="wsOperContextHandle">Handle for the the current web service operation context.</param>
    /// <param name="wsErrorHandle">Handle for the rich error info utility.</param>
    void SetSoapFault(
        const string &reason,
        const string &details,
        const char *action,
        const WS_OPERATION_CONTEXT *wsOperContextHandle,
        WS_ERROR *wsErrorHandle)
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
            err.RaiseExceptionApiError(hr, "WsGetOperationContextProperty",
                "Failed to retrieve heap object from web service operation context");

            WSHeap heap(wsHeapHandle); // wraps the heap handle into an object

            static const WS_XML_STRING faultCodeNamespace =
                WS_XML_STRING_VALUE("http://www.w3.org/2003/05/soap-envelope");

            static const WS_XML_STRING faultCodeLocalName = WS_XML_STRING_VALUE("Receiver");

            ////////////////
            // FAULT CODE

            WS_FAULT fault{};
            fault.code = heap.Alloc<WS_FAULT_CODE>();
            fault.code->subCode = nullptr;
            fault.code->value.ns = faultCodeNamespace;
            fault.code->value.localName = faultCodeLocalName;

            //////////////////
            // FAULT REASON

            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            auto why = transcoder.from_bytes(reason);

            fault.reasonCount = 1;
            fault.reasons = heap.Alloc<WS_FAULT_REASON>(fault.reasonCount);

            static const WS_STRING faultReasonLanguage = WS_STRING_VALUE(L"en");

            auto &reason = fault.reasons[0];
            reason.lang = faultReasonLanguage;
            reason.text.length = why.length();
            reason.text.chars = heap.Alloc<wchar_t>(why.length());

            memcpy(reason.text.chars,
                   why.data(),
                   why.length() * sizeof why[0]);

            // Record the SOAP fault into the error object:
            hr = WsSetFaultErrorProperty(
                wsErrorHandle,
                WS_FAULT_ERROR_PROPERTY_FAULT,
                &fault,
                sizeof fault
            );
            err.RaiseExceptionApiError(hr, "WsSetFaultErrorProperty",
                "Failed to set SOAP fault into rich error information");

            //////////////////
            // FAULT ACTION

            WS_XML_STRING actionForFault = WS_XML_STRING_VALUE(action);

            // Record the action for SOAP fault:   
            hr = WsSetFaultErrorProperty(
                wsErrorHandle,
                WS_FAULT_ERROR_PROPERTY_ACTION,
                &actionForFault,
                sizeof actionForFault
            );
            err.RaiseExceptionApiError(hr, "WsSetFaultErrorProperty",
                "Failed to set action for SOAP fault into rich error information");

            //////////////////
            // FAULT DETAIL

            // this structure describes what comes inside the detail element of the SOAP FAULT
            auto faultDetailDesc = GetFaultDetailDescription(heap);

            // this buffer is to receive what comes inside
            WS_XML_BUFFER *wsXmlBufferHandle;

            hr = WsCreateXmlBuffer(
                heap.GetHandle(),
                nullptr, 0,
                &wsXmlBufferHandle,
                err.GetHandle()
            );
            err.RaiseExceptionApiError(hr, "WsCreateXmlBuffer",
                "Failed  to create XML buffer for contents of SOAP fault details");

            // Serialize the exception detail into the XML buffer:

            WSXmlWriter xmlWriter(wsXmlBufferHandle);

            xmlWriter.WriteStartElement(
                faultDetailDescElemNamespace,
                faultDetailDescElemLocalName
            );

            if (!details.empty())
                xmlWriter.WriteText(details);

            xmlWriter.WriteEndElement();

            // Record the fault details into the error object:
            hr = WsSetFaultErrorDetail(
                wsErrorHandle,
                &faultDetailDesc,
                WS_WRITE_REQUIRED_POINTER,
                &wsXmlBufferHandle,
                sizeof wsXmlBufferHandle
            );
            err.RaiseExceptionApiError(hr, "WsSetFaultErrorDetail",
                "Failed to set details for SOAP fault into rich error information");
        }
        catch (IAppException &ex)
        {
            Logger::Write(ex, Logger::Priority::PRIO_ERROR);

            Logger::Write(
                "Previous failure prevented creation of SOAP fault response after service error",
                Logger::Priority::PRIO_CRITICAL
            );
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when creating SOAP fault response: " << ex.what();
            Logger::Write(oss.str(), Logger::Priority::PRIO_CRITICAL);
        }
    }

    /// <summary>
    /// Creates a SOAP fault response from an exception (service error)
    /// and record it as rich error information.
    /// </summary>
    /// <param name="operEx">The exception thrown by the web service operation implementation.</param>
    /// <param name="action">The SOAP action to which the fault refers.</param>
    /// <param name="wsOperContextHandle">Handle for the the current web service operation context.</param>
    /// <param name="wsErrorHandle">Handle for the rich error info utility.</param>
    void SetSoapFault(
        core::IAppException &operEx,
        const char *action,
        const WS_OPERATION_CONTEXT *wsOperContextHandle,
        WS_ERROR *wsErrorHandle)
    {
        SetSoapFault(
            operEx.What(),
            operEx.Details(),
            action,
            wsOperContextHandle,
            wsErrorHandle
        );
    }

    /// <summary>
    /// Helper for HTTP header authorization.
    /// From the current operation context, the windows token for the (at this point)
    /// already authenticated sender will be retrieved from the HTTP header.
    /// </summary>
    /// <param name="wsOperContextHandle">Handle for the the current web service operation context.</param>
    /// <param name="senderWinToken">Where to save the windows token for the authenticated sender.
    /// A null value is set when processing an empty message, which has no token.</param>
    /// <param name="wsErrorHandle">Handle for the rich error info utility.</param>
    /// <returns><see cref="STATUS_OKAY"/> if token retrieval was succesfull or no token
    /// was expected for the message, otherwise, <see cref="STATUS_FAIL"/>.</returns>
    bool HelpAuthorizeSender(
        const WS_OPERATION_CONTEXT *wsOperContextHandle,
        HANDLE *senderWinToken,
        WS_ERROR *wsErrorHandle)
    {
        CALL_STACK_TRACE;

        try
        {
            WSError err(wsErrorHandle);

            WS_MESSAGE *wsMessageHandle;
            auto hr = WsGetOperationContextProperty(
                wsOperContextHandle,
                WS_OPERATION_CONTEXT_PROPERTY_INPUT_MESSAGE,
                &wsMessageHandle,
                sizeof wsMessageHandle,
                err.GetHandle()
            );
            err.RaiseExceptionApiError(hr, "WsGetOperationContextProperty",
                "Failed to retrieve input message from web service operation context");

            WS_MESSAGE_STATE messageState;
            hr = WsGetMessageProperty(
                wsMessageHandle,
                WS_MESSAGE_PROPERTY_STATE,
                &messageState,
                sizeof messageState,
                err.GetHandle()
            );
            err.RaiseExceptionApiError(hr, "WsGetMessageProperty",
                "Failed to retrieve state from input message");

            if (messageState == WS_MESSAGE_STATE_EMPTY)
            {
                *senderWinToken = nullptr;
                return STATUS_OKAY;
            }

            hr = WsGetMessageProperty(
                wsMessageHandle,
                WS_MESSAGE_PROPERTY_HTTP_HEADER_AUTH_WINDOWS_TOKEN,
                senderWinToken,
                sizeof *senderWinToken,
                err.GetHandle()
            );
            err.RaiseExceptionApiError(hr, "WsGetMessageProperty",
                "Failed to retrieve Windows token from HTTP header of input message");

            return STATUS_OKAY;
        }
        catch (IAppException &ex)
        {
            SetSoapFault(ex, "AuthorizeSender", wsOperContextHandle, wsErrorHandle);
            Logger::Write(ex, Logger::Priority::PRIO_ERROR);
            return STATUS_FAIL;
        }
        catch (std::exception &ex)
        {
            SetSoapFault(
                "Failed to get Windows token from message sender for authorization",
                ex.what(),
                "AuthorizeSender",
                wsOperContextHandle,
                wsErrorHandle
            );

            std::ostringstream oss;
            oss << "Generic failure when attempting to get Windows "
                   "token from message sender for authorization: " << ex.what();

            Logger::Write(oss.str(), Logger::Priority::PRIO_CRITICAL);
            return STATUS_FAIL;
        }
    }

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd
