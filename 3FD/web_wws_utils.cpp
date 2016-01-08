#include "stdafx.h"
#include "web_wws_utils.h"

#include "callstacktracer.h"
#include "logger.h"
#include <array>

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

			////////////////////////////
			// Asynchronous Helper
			////////////////////////////

			/// <summary>
			/// The callback invoked when an asynchronous operation finishes.
			/// This may be invoked synchronously.
			/// </summary>
			/// <param name="hres">The HRESULT of status returned by the asynchronous call.</param>
			/// <param name="model">The asynchronous model from WWS API for the current call.</param>
			/// <param name="state">The state, which packs parameters for this callback.</param>
			void CALLBACK AsyncDoneCallback(HRESULT hres, WS_CALLBACK_MODEL model, void *state)
			{
				CALL_STACK_TRACE;

				auto &promise = *static_cast<std::promise<HRESULT> *> (state);

				try
				{
					promise.set_value(hres);
				}
				catch (std::future_error &ex)
				{
					std::ostringstream oss;
					oss << "Failed to retrieve result from asynchronous operation: "
						<< StdLibExt::GetDetailsFromFutureError(ex);

					AppException<std::runtime_error> appEx(oss.str());
					promise.set_exception(std::make_exception_ptr(appEx));
				}
			}

			/// <summary>
			/// Initializes a new instance of the <see cref="WSAsyncOper"/> class.
			/// </summary>
			WSAsyncOper::WSAsyncOper(size_t heapSize)
			try :
				m_promise(new std::promise<HRESULT>()),
				m_heap(heapSize),
				m_callReturn(WS_S_ASYNC)
			{
				m_future = m_promise->get_future();
			}
			catch (std::future_error &ex)
			{
				std::ostringstream oss;
				oss << "Failed when preparing for asynchronous operation: "
					<< StdLibExt::GetDetailsFromFutureError(ex);

				throw AppException<std::runtime_error>(oss.str());
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when preparing for asynchronous operation: " << ex.what();
				throw AppException<std::runtime_error>(oss.str());
			}

			/// <summary>
			/// Finalizes an instance of the <see cref="WSAsyncOper"/> class.
			/// </summary>
			WSAsyncOper::~WSAsyncOper()
			{
				delete m_promise;
			}

			/// <summary>
			/// Gets the context structure used by WWS API calls in asynchronous execution.
			/// </summary>
			/// <returns>
			/// A copy of the context structure to pass as parameter for the asynchronous call.
			/// </returns>
			WS_ASYNC_CONTEXT WSAsyncOper::GetContext()
			{
				WS_ASYNC_CONTEXT ctx;
				ctx.callback = AsyncDoneCallback;
				ctx.callbackState = m_promise;
				return ctx;
			}

			/// <summary>
			/// The HRESULT immediately returned by the call meant to be asynchronous.
			/// </summary>
			/// <param name="hres">The immediately returned HRESULT.</param>
			void WSAsyncOper::SetCallReturn(HRESULT hres)
			{
				m_callReturn = hres;
			}

			/// <summary>
			/// Waits for the result of the asynchronous operation.
			/// </summary>
			void WSAsyncOper::Wait() const
			{
				if (m_callReturn == WS_S_ASYNC)
					m_future.wait();
			}

			/// <summary>
			/// Gets the result of the asynchronous operation.
			/// </summary>
			/// <returns>
			/// The HRESULT of status returned at the end of the asynchronous operation.
			/// </returns>
			HRESULT WSAsyncOper::GetResult()
			{
				if (m_callReturn == WS_S_ASYNC)
				{
					try
					{
						return m_callReturn = m_future.get();
					}
					catch (IAppException &)
					{
						throw; /* just forward transported exceptions which
							   are known to have been already handled */
					}
					catch (std::future_error &ex)
					{
						std::ostringstream oss;
						oss << "Failed to retrieve result from asynchronous operation: "
							<< StdLibExt::GetDetailsFromFutureError(ex);

						throw AppException<std::runtime_error>(oss.str());
					}
				}
				else
					return m_callReturn;
			}

		}// end of namespace wws
	}// end of namespace web
}// end of namespace _3fd
