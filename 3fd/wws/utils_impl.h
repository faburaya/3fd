#ifndef WEB_WWS_UTILS_IMPL_H // header guard
#define WEB_WWS_UTILS_IMPL_H

#include "utils.h"

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

    WS_STRING ToWsString(const std::string_view &str, WSHeap &heap);

    WS_STRING ToWsString(const std::wstring_view &str, WSHeap &heap);


    //////////////////////
    // XML Handling
    //////////////////////

    extern const WS_XML_STRING faultDetailDescElemNamespace;

    extern const WS_XML_STRING faultDetailDescElemLocalName;

    WS_FAULT_DETAIL_DESCRIPTION GetFaultDetailDescription(WSHeap &heap);


    /// <summary>
    /// A wrapper for WS_XML_WRITER.
    /// </summary>
    class WSXmlWriter
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
        WSXmlWriter(WSXmlWriter &&ob) noexcept
            : m_wsXmlWriterHandle(ob.m_wsXmlWriterHandle)
        {
            ob.m_wsXmlWriterHandle = nullptr;
        }

        WSXmlWriter(const WSXmlWriter &) = delete;

        void WriteStartElement(
            const WS_XML_STRING &ns,
            const WS_XML_STRING &localName);

        void WriteEndElement();

        void WriteText(const string &content);
    };


    /// <summary>
    /// A wrapper for WS_XML_READER.
    /// </summary>
    class WSXmlReader
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
        WSXmlReader(WSXmlReader &&ob) noexcept
            : m_wsXmlReaderHandle(ob.m_wsXmlReaderHandle)
        {
            ob.m_wsXmlReaderHandle = nullptr;
        }

        WSXmlReader(const WSXmlReader &) = delete;

        void ReadStartElement(
            const WS_XML_STRING &ns,
            const WS_XML_STRING &localName);

        void ReadEndElement();

        void ReadText(std::vector<char> &utf8text);
    };

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd

#endif // end of header guard
