#ifndef WEB_WWS_UTILS_H // header guard
#define WEB_WWS_UTILS_H

#include "base.h"
#include "exceptions.h"
#include <WebServices.h>

#include <vector>
#include <string>
#include <future>

#define WS_HEAP_NEW(HEAPOBJ, TYPE, INITIALIZER) new (HEAPOBJ.Alloc<TYPE>()) TYPE INITIALIZER

namespace _3fd
{
	using std::string;

	namespace web
	{
		namespace wws
        {
            /// <summary>
            /// A heap provides precise control over memory allocation when producing or 
            /// consuming messages and when needing to allocate various other API structures.
            /// </summary>
            class WSHeap : notcopiable
            {
            private:

                WS_HEAP *m_wsHeapHandle;
                bool m_allowRelease;

            public:

                WSHeap(WS_HEAP *wsHeapHandle);

                WSHeap(size_t nBytes);

                ~WSHeap();

                /// <summary>
                /// Initializes a new instance of the <see cref="WSHeap"/> class using move semantics.
                /// </summary>
                /// <param name="ob">The object whose resources will be stolen from.</param>
                WSHeap(WSHeap &&ob) :
                    m_wsHeapHandle(ob.m_wsHeapHandle),
                    m_allowRelease(ob.m_allowRelease)
                {
                    ob.m_wsHeapHandle = nullptr;
                }

                void Reset() throw();

                void *Alloc(size_t qtBytes);

                template <typename Type> Type *Alloc()
                {
                    return static_cast<Type *> (Alloc(sizeof(Type)));
                }

                template <typename Type> Type *Alloc(size_t qtObjects)
                {
                    _ASSERTE(qtObjects > 0);
                    return static_cast<Type *> (Alloc(sizeof(Type) * qtObjects));
                }

                /// <summary>
                /// Gets the heap handle.
                /// </summary>
                /// <returns>The handle for the opaque handle object.</returns>
                WS_HEAP *GetHandle() { return m_wsHeapHandle; }
            };

			///////////////////////////
			// String manipulation
			///////////////////////////

			WS_XML_STRING ToWsXmlString(const string &str);

			WS_XML_STRING *ToWsXmlString(const string &str, WSHeap &heap);

			WS_STRING ToWsString(const string &str, WSHeap &heap);

			////////////////////////
			// Error Handling
			////////////////////////

			/// <summary>
			/// A reusable object model capable to hold rich error information.
			/// </summary>
			class WSError : notcopiable
			{
			private:

				WS_ERROR *m_wsErrorHandle;

				void Initialize();
				void Reset();

				void RaiseExClientSoapFault(HRESULT hres, const char *message, WS_HEAP *wsHeapHandle);

			public:

				WSError();
				~WSError();

				/// <summary>
				/// Initializes a new instance of the <see cref="WSError"/>
				/// class using move semantics.
				/// </summary>
				/// <param name="ob">The objects whose resources will be stolen from.</param>
				WSError(WSError &&ob) :
					m_wsErrorHandle(ob.m_wsErrorHandle)
				{
					ob.m_wsErrorHandle = nullptr;
				}

				void RaiseExceptionApiError(
					HRESULT hres,
					const char *funcName,
					const char *message);

				void LogApiError(
					HRESULT hres,
					const char *funcName,
					const char *message) throw();

                void RaiseExClientNotOK(
					HRESULT hres,
					const char *message,
					WSHeap &heap);

				WS_ERROR *GetHandle();
			};

			//////////////////////
			// XML Handling
			//////////////////////

			extern const WS_XML_STRING faultDetailDescElemNamespace;

			extern const WS_XML_STRING faultDetailDescElemLocalName;

			WS_FAULT_DETAIL_DESCRIPTION GetFaultDetailDescription(WSHeap &heap);

			/// <summary>
			/// A wrapper for WS_XML_WRITER.
			/// </summary>
			class WSXmlWriter : notcopiable
			{
			private:

				WS_XML_WRITER *m_wsXmlWriterHandle;

			public:

				WSXmlWriter(WS_XML_BUFFER *wsXmlBufferHandle);

				~WSXmlWriter();

				/// <summary>
				/// Initializes a new instance of the <see cref="WSXmlWriter"/> class using move semantics.
				/// </summary>
				/// <param name="ob">The object whose resources will be stolen from.</param>
				WSXmlWriter(WSXmlWriter &&ob) :
					m_wsXmlWriterHandle(ob.m_wsXmlWriterHandle)
				{
					ob.m_wsXmlWriterHandle = nullptr;
				}

				void WriteStartElement(
					const WS_XML_STRING &ns,
					const WS_XML_STRING &localName);

				void WriteEndElement();

				void WriteText(const string &content);
			};

			/// <summary>
			/// A wrapper for WS_XML_READER.
			/// </summary>
			class WSXmlReader : notcopiable
			{
			private:

				WS_XML_READER *m_wsXmlReaderHandle;

			public:

				WSXmlReader(WS_XML_BUFFER *wsXmlBufferHandle);

				~WSXmlReader();

				/// <summary>
				/// Initializes a new instance of the <see cref="WSXmlReader"/> class
				/// using move semantics.
				/// </summary>
				/// <param name="ob">The object whose resources will be stolen.</param>
				WSXmlReader(WSXmlReader &&ob) :
					m_wsXmlReaderHandle(ob.m_wsXmlReaderHandle)
				{
					ob.m_wsXmlReaderHandle = nullptr;
				}

				void ReadStartElement(
					const WS_XML_STRING &ns,
					const WS_XML_STRING &localName);

				void ReadEndElement();

				void ReadText(std::vector<char> &utf8text);
			};

			////////////////////////////
			// Asynchronous Helper
			////////////////////////////

			void CALLBACK AsyncDoneCallback(HRESULT hres, WS_CALLBACK_MODEL model, void *state);

			/// <summary>
			/// Helper class for asynchronous operations with WWS API.
			/// </summary>
			class WSAsyncOper : notcopiable
			{
			private:

				std::promise<HRESULT> *m_promise;
				std::future<HRESULT> m_future;

				WSHeap m_heap;
				WSError m_richErrorInfo;

				/// <summary>
				/// The HRESULT immediately returned by the call meant to be asynchronous.
				/// </summary>
				HRESULT m_callReturn;

				void Wait() const;

				HRESULT GetResult();

				friend void CALLBACK AsyncDoneCallback(HRESULT, WS_CALLBACK_MODEL, void *);

				// Forbidden operation
				WSAsyncOper &operator =(const WSAsyncOper &) { return *this; }

			public:

				WSAsyncOper(size_t heapSize);

				~WSAsyncOper();

				/// <summary>
				/// Initializes a new instance of the <see cref="WSAsyncOper"/> class
				/// using move semantics.
				/// </summary>
				/// <param name="ob">The object whose resources will be stolen.</param>
				WSAsyncOper(WSAsyncOper &&ob) : 
					m_promise(ob.m_promise),
					m_future(std::move(ob.m_future)),
					m_heap(std::move(ob.m_heap)),
					m_richErrorInfo(std::move(ob.m_richErrorInfo)),
					m_callReturn(ob.m_callReturn)
				{
					ob.m_promise = nullptr;
				}

				WS_ASYNC_CONTEXT GetContext();

				void SetCallReturn(HRESULT hres);

				/// <summary>
				/// Gets the heap dedicated for this asynchronous operation.
				/// </summary>
				/// <returns>The heap handle.</returns>
				WS_HEAP *GetHeapHandle() { return m_heap.GetHandle(); }

				/// <summary>
				/// Gets the helper for rich error information
				/// dedicated to this asynchronous operation.
				/// </summary>
				/// <returns>
				/// The handle for the rich error information helper.
				/// </returns>
				WS_ERROR *GetErrHelperHandle() { return m_richErrorInfo.GetHandle(); }

				/// <summary>
				/// Waits and checks the result code from a proxy asynchronous operation.
				/// When the given result code means "NOT OKAY", populate the object with rich information
				/// regarding the error, than raises an exception with such content.
				/// </summary>
				/// <param name="message">The base message for the error.</param>
				void RaiseExClientNotOK(const char *message)
				{
					m_richErrorInfo.RaiseExClientNotOK(GetResult(), message, m_heap);
				}
			};

		}// end of namespace wws
	}// end of namespace web
}// end of namespace _3fd

#endif // end of header guard
