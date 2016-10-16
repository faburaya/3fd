#ifndef WEB_WWS_UTILS_IMPL_H // header guard
#define WEB_WWS_UTILS_IMPL_H

#include "web_wws_utils.h"

#define WS_HEAP_NEW(HEAPOBJ, TYPE, INITIALIZER) (new (HEAPOBJ.Alloc<TYPE>()) TYPE INITIALIZER)

namespace _3fd
{
using std::string;

namespace web
{
namespace wws
{
    ///////////////////////////
    // String manipulation
    ///////////////////////////

    WS_XML_STRING ToWsXmlString(const string &str);

    WS_XML_STRING *ToWsXmlString(const string &str, WSHeap &heap);

    WS_STRING ToWsString(const string &str, WSHeap &heap);

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

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd

#endif // end of header guard
