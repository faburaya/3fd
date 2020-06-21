#include "pch.h"
#include "utils_impl.h"
#include <3fd/core/callstacktracer.h>
#include <3fd/core/logger.h>
#include <array>

namespace _3fd
{
namespace web
{
namespace wws
{
    using namespace _3fd::core;

    /////////////////////////////
    // WSXmlWriter Class
    /////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="WSXmlWriter"/> class.
    /// </summary>
    /// <param name="wsXmlBufferHandle">
    /// The handle for the XML buffer this instance will write to.
    /// </param>
    WSXmlWriter::WSXmlWriter(WS_XML_BUFFER *wsXmlBufferHandle)
    try :
        m_wsXmlWriterHandle(nullptr)
    {
        CALL_STACK_TRACE;

        BOOL allowFragment = TRUE;

        std::array<WS_XML_WRITER_PROPERTY, 1> properties =
        {
            WS_XML_WRITER_PROPERTY {
                WS_XML_WRITER_PROPERTY_ALLOW_FRAGMENT,
                &allowFragment,
                sizeof allowFragment
            }
        };

        WSError err;
        auto hr = WsCreateWriter(
            properties.data(),
            properties.size(),
            &m_wsXmlWriterHandle,
            err.GetHandle()
        );
        err.RaiseExceptionApiError(hr, "WsCreateWriter", "Failed to create XML writer");

        hr = WsSetOutputToBuffer(
            m_wsXmlWriterHandle,
            wsXmlBufferHandle,
            nullptr, 0,
            err.GetHandle()
        );
        err.RaiseExceptionApiError(hr, "WsSetOutputToBuffer", "Failed to set output to XML buffer");
    }
    catch (...)
    {
        if (m_wsXmlWriterHandle != nullptr)
            WsFreeWriter(m_wsXmlWriterHandle);
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="WSXmlWriter"/> class.
    /// </summary>
    WSXmlWriter::~WSXmlWriter()
    {
        if (m_wsXmlWriterHandle != nullptr)
            WsFreeWriter(m_wsXmlWriterHandle);
    }

    /// <summary>
    /// Writes into the buffer a start element.
    /// </summary>
    /// <param name="ns">The element namespace.</param>
    /// <param name="localName">The element local name.</param>
    void WSXmlWriter::WriteStartElement(
        const WS_XML_STRING &ns,
        const WS_XML_STRING &localName)
    {
        CALL_STACK_TRACE;
        WSError err;
        auto hr = WsWriteStartElement(
            m_wsXmlWriterHandle,
            nullptr,
            &localName,
            &ns,
            err.GetHandle()
        );
        err.RaiseExceptionApiError(hr, "WsWriteStartElement",
            "Failed to write start element into XML buffer");
    }

    /// <summary>
    /// Writes into the buffer an end element.
    /// </summary>
    void WSXmlWriter::WriteEndElement()
    {
        CALL_STACK_TRACE;
        WSError err;
        auto hr = WsWriteEndElement(
            m_wsXmlWriterHandle,
            err.GetHandle()
        );
        err.RaiseExceptionApiError(hr, "WsWriteEndElement",
            "Failed to write end element into XML buffer");
    }

    /// <summary>
    /// Writes text content into the buffer.
    /// </summary>
    /// <param name="content">The text content, UTF-8 encoded.</param>
    void WSXmlWriter::WriteText(const string &content)
    {
        CALL_STACK_TRACE;
        WSError err;
        auto hr = WsWriteCharsUtf8(
            m_wsXmlWriterHandle,
            (const BYTE *)content.data(),
            content.length(),
            err.GetHandle()
        );
        err.RaiseExceptionApiError(hr, "WsWriteCharsUtf8",
            "Failed to write text into XML buffer");
    }

    /////////////////////////////
    // WSXmlReader Class
    /////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="WSXmlReader"/> class.
    /// </summary>
    /// <param name="wsXmlBufferHandle">
    /// The handle for the XML buffer this instance will read from.
    /// </param>
    WSXmlReader::WSXmlReader(WS_XML_BUFFER *wsXmlBufferHandle)
    try :
        m_wsXmlReaderHandle(nullptr)
    {
        CALL_STACK_TRACE;

        BOOL allowFragment = TRUE;

        std::array<WS_XML_READER_PROPERTY, 1> properties =
        {
            WS_XML_READER_PROPERTY {
                WS_XML_READER_PROPERTY_ALLOW_FRAGMENT,
                &allowFragment,
                sizeof allowFragment
            }
        };

        WSError err;
        auto hr = WsCreateReader(
            properties.data(),
            properties.size(),
            &m_wsXmlReaderHandle,
            err.GetHandle()
        );
        err.RaiseExceptionApiError(hr, "WsCreateReader", "Failed to create XML reader");

        hr = WsSetInputToBuffer(
            m_wsXmlReaderHandle,
            wsXmlBufferHandle,
            nullptr, 0,
            err.GetHandle()
        );
        err.RaiseExceptionApiError(hr, "WsSetInputToBuffer", "Failed to set input to XML buffer");
    }
    catch (...)
    {
        if (m_wsXmlReaderHandle != nullptr)
            WsFreeReader(m_wsXmlReaderHandle);
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="WSXmlReader"/> class.
    /// </summary>
    WSXmlReader::~WSXmlReader()
    {
        if (m_wsXmlReaderHandle != nullptr)
            WsFreeReader(m_wsXmlReaderHandle);
    }

    /// <summary>
    /// Reads the start element.
    /// </summary>
    /// <param name="ns">The namespace expected for the element being read.</param>
    /// <param name="localName">The local name expected for the element being read.</param>
    void WSXmlReader::ReadStartElement(
        const WS_XML_STRING &ns,
        const WS_XML_STRING &localName)
    {
        CALL_STACK_TRACE;

        // First find the specified start element and advance to it:
        WSError err;
        BOOL found;
        auto hr = WsReadToStartElement(
            m_wsXmlReaderHandle,
            &localName,
            &ns,
            &found,
            err.GetHandle()
        );
        err.RaiseExceptionApiError(hr, "WsReadToStartElement",
            "Failed to read start element from XML buffer");

        // Then, read the start element moving the reader past it:
        hr = WsReadStartElement(
            m_wsXmlReaderHandle,
            err.GetHandle()
        );
        err.RaiseExceptionApiError(hr, "WsReadStartElement",
            "Failed to read start element from XML buffer");
    }

    /// <summary>
    /// Reads the end element.
    /// </summary>
    void WSXmlReader::ReadEndElement()
    {
        CALL_STACK_TRACE;
        WSError err;
        auto hr = WsReadEndElement(
            m_wsXmlReaderHandle,
            err.GetHandle()
        );
        err.RaiseExceptionApiError(hr, "ReadEndElement",
            "Failed to read end element from XML buffer");
    }

    /// <summary>
    /// Reads text content from the buffer.
    /// </summary>
    /// <param name="utf8text">
    /// Where to save the read UTF-8 encoded text.
    /// </param>
    void WSXmlReader::ReadText(std::vector<char> &utf8text)
    {
        CALL_STACK_TRACE;

        try
        {
            // Resize output to minimum size:
            const ULONG chunkSize(256);
            if (utf8text.size() < chunkSize)
                utf8text.resize(chunkSize);

            WSError err;
            ULONG nBytesRead;

            auto availableSize = static_cast<ULONG> (utf8text.size());
            auto toVecMem = reinterpret_cast<BYTE *> (utf8text.data());
                    
            while (true)
            {
                // Read from XML buffer:
                auto hr = WsReadCharsUtf8(
                    m_wsXmlReaderHandle,
                    toVecMem,
                    availableSize,
                    &nBytesRead,
                    err.GetHandle()
                );
                err.RaiseExceptionApiError(hr, "WsReadCharsUtf8",
                    "Failed to read text from XML buffer");

                if (nBytesRead > 0)
                    availableSize -= nBytesRead;
                else
                {
                    utf8text.resize(utf8text.size() - availableSize);
                    return;
                }

                /* If necessary, expand vector to fit another
                chunk of data from XML buffer: */
                if (availableSize == 0)
                {
                    auto offset = toVecMem - reinterpret_cast<BYTE *> (utf8text.data());
                    auto newSize = (1 + utf8text.size() / chunkSize) * chunkSize;
                    availableSize = newSize - utf8text.size();
                    utf8text.resize(newSize);
                    toVecMem = reinterpret_cast<BYTE *> (utf8text.data()) + offset;
                }

                toVecMem += nBytesRead;
            }// end loop
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions for errors known to have already been handled
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when reading text from XML buffer: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd
