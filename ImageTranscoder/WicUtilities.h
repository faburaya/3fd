#ifndef WICUTILITIES_H // header guard
#define WICUTILITIES_H

#include "3FD\base.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include <comutil.h>

#ifdef _3FD_PLATFORM_WIN32API
#   include <MsXml6.h>
#   define XMLSTR               BSTR
#   define XmlStrWrap           _bstr_t
#   define XmlDom               ComPtr<IXMLDOMDocument2>
#   define XmlNode              ComPtr<IXMLDOMNode>
#   define XmlNodeList          ComPtr<IXMLDOMNodeList>
#   define XmlNamedNodeMap      ComPtr<IXMLDOMNamedNodeMap>

#elif defined _3FD_PLATFORM_WINRT
#   define XMLSTR           Platform::String^
#   define XmlStrWrap       Platform::StringReference
#   define XmlDom           Windows::Data::Xml::Dom::XmlDocument^
#   define XmlNode          Windows::Data::Xml::Dom::IXmlNode^
#   define XmlNodeList      Windows::Data::Xml::Dom::XmlNodeList^
#   define XmlNamedNodeMap  Windows::Data::Xml::Dom::XmlNamedNodeMap^
#endif

#define CHECK(COM_CALL_EXPR) if ((hr = (COM_CALL_EXPR)) < 0) { WWAPI::RaiseHResultException(hr, "Unexpected error in COM interface call", #COM_CALL_EXPR); }

#undef min
#undef max

#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <limits>

namespace application
{
    using namespace _3fd;
    using namespace _3fd::core;

    using namespace Microsoft::WRL;

    ///////////////////////
    // VARIANT handling
    ///////////////////////

    const wchar_t *UnwrapCString(_variant_t variant);

#ifdef _3FD_PLATFORM_WIN32API

    /// <summary>
    /// Gets the value from a VARIANT.
    /// </summary>
    /// <param name="variant">The variant object.</param>
    /// <returns>A reference to the value held by the VARIANT.</returns>
    template <typename ValType>
    ValType GetValueFrom(_variant_t &variant)
    {
        static_cast<false>; // causes compilation failure in case this generic implementation gets compiled
    }

    template <> BSTR GetValueFrom<BSTR>(_variant_t &variant);
    template <> bool GetValueFrom<bool>(_variant_t &variant);
    template <> uint16_t GetValueFrom<uint16_t>(_variant_t &variant);
    template <> uint32_t GetValueFrom<uint32_t>(_variant_t &variant);
    template <> int16_t GetValueFrom<int16_t>(_variant_t &variant);
    template <> int32_t GetValueFrom<int32_t>(_variant_t &variant);

#endif

    /// <summary>
    /// PROPVARIANT wrapper class
    /// </summary>
    class _propvariant_t : notcopiable, public PROPVARIANT
    {
    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="_propvariant_t"/> class.
        /// </summary>
        _propvariant_t()
        {
            PropVariantInit(this);
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="_propvariant_t"/> class.
        /// </summary>
        ~_propvariant_t()
        {
            try
            {
                HRESULT hr;
                if (FAILED(hr = PropVariantClear(this)))
                    WWAPI::RaiseHResultException(hr, "Failed to release resouces from PROPVARIANT", "PropVariantClear");
            }
            catch (HResultException &ex)
            {
                Logger::Write(ex, Logger::PRIO_CRITICAL);
            }
        }
    };


    /////////////////////
    // XML handling
    /////////////////////

    /// <summary>
    /// Gets the XML attribute value.
    /// </summary>
    /// <param name="attributes">The attributes.</param>
    /// <param name="attrName">Name of the attribute.</param>
    /// <param name="value">A reference to the variable to receive the value.</param>
    template <typename ValType>
    void GetAttributeValue(XmlNamedNodeMap attributes,
                           XMLSTR attrName,
                           ValType &value)
    {
#   ifdef _3FD_PLATFORM_WIN32API

        CALL_STACK_TRACE;

        HRESULT hr;
        _variant_t variant;
        ComPtr<IXMLDOMNode> attrNode;
        CHECK(attributes->getNamedItem(attrName, attrNode.GetAddressOf()));
        CHECK(attrNode->get_nodeValue(variant.GetAddress()));
        _ASSERTE(variant.vt == VT_BSTR);
        value = GetValueFrom<ValType>(variant);

#   elif defined _3FD_PLATFORM_WINRT

        auto attrNode = attributes->GetNamedItem(attrName);
        _ASSERTE(attrNode != nullptr);
        auto boxedVal = attrNode->NodeValue;
        value = safe_cast<ValType> (boxedVal);

#   endif
    }

// XML handling for Win32 (MSXML6):
#ifdef _3FD_PLATFORM_WIN32API

    BSTR ExtractBstrFrom(_variant_t &wrappedVar);

    _bstr_t GetAttributeValueHash(XmlNamedNodeMap attributes,
                                  BSTR attrName,
                                  uint32_t &hash);

    XmlNodeList XmlSelectNodes(XmlDom dom, BSTR xpath);

    long XmlGetLength(XmlNodeList nodes);

    XmlNode XmlGetItem(XmlNodeList nodes, long index);

    _variant_t XmlGetNodeValue(XmlNode node);

    _bstr_t XmlGetXml(XmlNode node);

    XmlNodeList XmlGetChildNodes(XmlNode node);

    XmlNamedNodeMap XmlGetAttributes(XmlNode elemNode);

    XmlNode XmlGetNamedItem(XmlNamedNodeMap attributes, BSTR name);

    BSTR UnwrapCString(_bstr_t str);

    BSTR UnwrapCString(BSTR str);

// XML handling for WinRT:
#elif defined _3FD_PLATFORM_WINRT

    using Platform::Object;
    using Platform::String;
    using Platform::StringReference;
    
    String ^GetAttributeValueHash(XmlNamedNodeMap attributes,
                                  String ^attrName,
                                  uint32_t &hash);

    XmlNodeList XmlSelectNodes(XmlDom dom, String ^xpath);

    unsigned int XmlGetLength(XmlNodeList nodes);

    XmlNode XmlGetItem(XmlNodeList nodes, unsigned int index);

    Object ^XmlGetNodeValue(XmlNode node);

    String ^XmlGetXml(XmlNode node);

    XmlNodeList XmlGetChildNodes(XmlNode node);

    XmlNamedNodeMap XmlGetAttributes(XmlNode elemNode);

    XmlNode XmlGetNamedItem(XmlNamedNodeMap attributes, String ^name);

    const wchar_t *UnwrapCString(String ^str);

    const wchar_t *UnwrapCString(Object ^obj);

#endif

    ////////////////
    // Hashing
    ////////////////

    uint32_t HashGuid(const GUID &guid);

    uint64_t MakeKey(uint32_t low, uint32_t high);

    uint64_t MakeKey(const GUID &srcFormatGuid, const GUID &destFormatGuid);

    uint32_t HashName(XMLSTR str);
    
}// end of namespace application

#endif // end of header guard
