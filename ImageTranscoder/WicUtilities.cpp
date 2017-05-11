#include "stdafx.h"
#include "WicUtilities.h"
#include <codecvt>

namespace application
{
    ///////////////////////
    // VARIANT handling
    ///////////////////////

    const wchar_t *UnwrapCString(_variant_t variant)
    {
        _ASSERTE(variant.vt == VT_BSTR);
        return variant.bstrVal;
    }

#ifdef _3FD_PLATFORM_WIN32API

    template <> BSTR GetValueFrom<BSTR>(_variant_t &variant)
    {
        return ExtractBstrFrom(variant);
    }

    template <> bool GetValueFrom<bool>(_variant_t &variant)
    {
        if (wcscmp(variant.bstrVal, L"true") == 0 || wcscmp(variant.bstrVal, L"1") == 0)
            return true;
        else if (wcscmp(variant.bstrVal, L"false") == 0 || wcscmp(variant.bstrVal, L"0") == 0)
            return false;
        else
            throw AppException<std::logic_error>("Validation of XML configuraton file is broken: not a valid boolean!");
    }

    template <> uint16_t GetValueFrom<uint16_t>(_variant_t &variant)
    {
        _set_errno(0);
        unsigned long integer = wcstoul(variant.bstrVal, nullptr, 10);

        if (integer == 0 && wcscmp(variant.bstrVal, L"0") != 0)
            throw AppException<std::logic_error>("Validation of XML configuraton file is broken: not a valid uint16!");
        else if (errno == ERANGE || std::numeric_limits<uint16_t>::max() < integer)
            throw AppException<std::logic_error>("Validation of XML configuraton file is broken: out of uint16 range!");
        else
            return static_cast<uint16_t> (integer);
    }

    template <> uint32_t GetValueFrom<uint32_t>(_variant_t &variant)
    {
        _set_errno(0);
        unsigned long integer = wcstoul(variant.bstrVal, nullptr, 10);

        if (integer == 0 && wcscmp(variant.bstrVal, L"0") != 0)
            throw AppException<std::logic_error>("Validation of XML configuraton file is broken: not a valid uint32!");
        else if (errno == ERANGE)
            throw AppException<std::logic_error>("Validation of XML configuraton file is broken: out of uint32 range!");
        else
            return static_cast<uint32_t> (integer);
    }

    template <> int16_t GetValueFrom<int16_t>(_variant_t &variant)
    {
        _set_errno(0);
        long integer = wcstol(variant.bstrVal, nullptr, 10);

        if (integer == 0 && wcscmp(variant.bstrVal, L"0") != 0)
            throw AppException<std::logic_error>("Validation of XML configuraton file is broken: not a valid int16!");
        else if (errno == ERANGE || std::numeric_limits<int16_t>::max() < integer || std::numeric_limits<int16_t>::min() > integer)
            throw AppException<std::logic_error>("Validation of XML configuraton file is broken: out of int16 range!");
        else
            return static_cast<int16_t> (integer);
    }

    template <> int32_t GetValueFrom<int32_t>(_variant_t &variant)
    {
        _set_errno(0);
        long integer = wcstol(variant.bstrVal, nullptr, 10);

        if (integer == 0 && wcscmp(variant.bstrVal, L"0") != 0)
            throw AppException<std::logic_error>("Validation of XML configuraton file is broken: not a valid int32!");
        else if (errno == ERANGE)
            throw AppException<std::logic_error>("Validation of XML configuraton file is broken: out of int32 range!");
        else
            return static_cast<int32_t> (integer);
    }

    /// <summary>
    /// Extracts the BSTR from a COM wrapped VARIANT, bypassing its deallocation.
    /// </summary>
    /// <param name="wrappedVar">The wrapped VARIANT.</param>
    /// <returns>The extracted BSTR.</returns>
    BSTR ExtractBstrFrom(_variant_t &wrappedVar)
    {
        _ASSERTE(wrappedVar.vt == VT_BSTR);
        VARIANT rawVar;
        rawVar = wrappedVar.Detach();
        return rawVar.bstrVal;
    }

#endif

    /////////////////////
    // XML handling
    /////////////////////

#ifdef _3FD_PLATFORM_WIN32API

    /* MSXML parsing validates the content against the referenced schema (XSD), thus the calls for
       browsing the DOM are not supposed to fail. Their results are only checked because failures
       such as running out of memory can always happen. */

    XmlNodeList XmlSelectNodes(XmlDom dom, BSTR xpath)
    {
        HRESULT hr;
        XmlNodeList nodes;
        CHECK(dom->selectNodes(xpath, nodes.GetAddressOf()));
        return nodes;
    }

    long XmlGetLength(XmlNodeList nodes)
    {
        HRESULT hr;
        long nodeCount;
        CHECK(nodes->get_length(&nodeCount));
        return nodeCount;
    }

    XmlNode XmlGetItem(XmlNodeList nodes, long index)
    {
        HRESULT hr;
        XmlNode node;
        CHECK(nodes->get_item(index, node.GetAddressOf()));
        return node;
    }

    _variant_t XmlGetNodeValue(XmlNode node)
    {
        HRESULT hr;
        _variant_t variant;
        CHECK(node->get_nodeValue(variant.GetAddress()));
        _ASSERTE(variant.vt == VT_BSTR);
        return variant;
    }

    _bstr_t XmlGetXml(XmlNode node)
    {
        _bstr_t content;
        node->get_xml(content.GetAddress());
        return content;
    }

    XmlNodeList XmlGetChildNodes(XmlNode node)
    {
        HRESULT hr;
        XmlNodeList nodes;
        CHECK(node->get_childNodes(nodes.GetAddressOf()));
        return nodes;
    }

    XmlNamedNodeMap XmlGetAttributes(XmlNode elemNode)
    {
        HRESULT hr;
        XmlNamedNodeMap attributes;
        CHECK(elemNode->get_attributes(attributes.GetAddressOf()));
        return attributes;
    }

    XmlNode XmlGetNamedItem(XmlNamedNodeMap attributes, BSTR name)
    {
        HRESULT hr;
        XmlNode node;
        CHECK(attributes->getNamedItem(name, node.GetAddressOf()));
        return node;
    }

    BSTR UnwrapCString(_bstr_t str) { return str.GetBSTR(); }

    BSTR UnwrapCString(BSTR str) { return str; } // no-op

    /// <summary>
    /// Gets the XML attribute value as a hash.
    /// </summary>
    /// <param name="attributes">The attributes.</param>
    /// <param name="attrName">Name of the attribute.</param>
    /// <param name="value">A reference to the variable to receive the value.</param>
    /// <returns>The originally parsed string value.</returns>
    _bstr_t GetAttributeValueHash(XmlNamedNodeMap attributes,
                                  BSTR attrName,
                                  uint32_t &hash)
    {
        BSTR value;
        GetAttributeValue(attributes, attrName, value);
        hash = HashName(value);
        return _bstr_t(value, false);
    }

#elif defined _3FD_PLATFORM_WINRT

    /// <summary>
    /// Gets the XML attribute value as a hash.
    /// </summary>
    /// <param name="attributes">The attributes.</param>
    /// <param name="attrName">Name of the attribute.</param>
    /// <param name="value">A reference to the variable to receive the value.</param>
    /// <returns>The originally parsed string value.</returns>
    String ^GetAttributeValueHash(XmlNamedNodeMap attributes,
                                  String ^attrName,
                                  uint32_t &hash)
    {
        String ^value;
        GetAttributeValue(attributes, attrName, value);
        hash = HashName(value);
        return value;
    }

    /* Windows Runtime XML parsing does not support XSD validation, neither does it need
       that, because XML configuration for metadata mapping is taken as valid (after the
       application tests that take place before shipping it to Windows Store.)
       Once installed in a device, it can only be modified by the developer in another
       release. Thus, the thrown exceptions are meant to help the developer to debug. */

    // XML DOM method to select nodes
    XmlNodeList XmlSelectNodes(XmlDom dom, String ^xpath) { return dom->SelectNodes(xpath); }

    // XML DOM method to get nodes list lenght
    unsigned int XmlGetLength(XmlNodeList nodes) { return nodes->Length; }

    // XML DOM method to get indexed item in list of nodes
    XmlNode XmlGetItem(XmlNodeList nodes, unsigned int index) { return nodes->Item(index); }

    // XML DOM method to get node value
    Object ^XmlGetNodeValue(XmlNode node)
    {
        auto value = node->NodeValue;

        if (value == nullptr)
        {
            std::wostringstream woss;
            woss << L"Could not get value from XML node '" << node->NodeName->Data()
                 << L"' in\r\n" << node->GetXml()->Data();

            std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
            throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
        }
        
        return value;
    }

    // XML DOM method to get XML content of node
    String ^XmlGetXml(XmlNode node) { return node->GetXml(); }

    // XML DOM method to get child nodes
    XmlNodeList XmlGetChildNodes(XmlNode node) { return node->ChildNodes; }

    // XML DOM method to get attributes from node
    XmlNamedNodeMap XmlGetAttributes(XmlNode node)
    {
        auto attributes = node->Attributes;

        if (attributes == nullptr)
        {
            std::wostringstream woss;
            woss << L"Cannot get attributes from XML node '" << node->NodeName->Data()
                 << L"' in\r\n" << node->GetXml()->Data();

            std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
            throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
        }

        return attributes;
    }

    // XML DOM method to get a named item from a list of attributes
    XmlNode XmlGetNamedItem(XmlNamedNodeMap attributes, String ^name)
    {
        auto item = attributes->GetNamedItem(name);

        if (item == nullptr)
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
            std::wostringstream woss;
            woss << L"Cannot get item '" << name->Data() << L"' from list of attributes";
            throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
        }

        return item;
    }

    const wchar_t *UnwrapCString(String ^str)
    {
        return str->Data();
    }

    const wchar_t *UnwrapCString(Object ^obj)
    {
        auto str = safe_cast<String ^> (obj);
        return str->Data();
    }

#endif

    /// <summary>
    /// Hashes the unique identifier using FNV1a algorithm.
    /// </summary>
    /// <param name="guid">The unique identifier.</param>
    /// <returns>The GUID hashed to an unsigned 32 bits integer.</returns>
    uint32_t HashGuid(const GUID &guid)
    {
        uint32_t hash(2166136261);

        auto data = (uint8_t *)&guid;
        for (int idx = 0; idx < sizeof guid; ++idx)
        {
            hash = hash ^ data[idx];
            hash = static_cast<uint32_t> (hash * 16777619ULL);
        }

        return hash;
    }

    /// <summary>
    /// Makes the key by concatenating 2 hash values.
    /// </summary>
    /// <param name="low">The low part of the key.</param>
    /// <param name="high">The high part of the key.</param>
    /// <returns>The key as a 64 bits long unsigned integer.</returns>
    uint64_t MakeKey(uint32_t low, uint32_t high)
    {
        struct { uint32_t low, high; } key;
        key.low = low;
        key.high = high;
        return *reinterpret_cast<uint64_t *> (&key);
    }

    /// <summary>
    /// Makes a key given the source and destination format GUID's.
    /// </summary>
    /// <param name="srcFormatGuid">The source format unique identifier.</param>
    /// <param name="destFormatGuid">The destination format unique identifier.</param>
    /// <returns>The key as a 64 bits long unsigned integer.</returns>
    uint64_t MakeKey(const GUID &srcFormatGuid, const GUID &destFormatGuid)
    {
        return MakeKey(HashGuid(srcFormatGuid), HashGuid(destFormatGuid));
    }

    /// <summary>
    /// Hashes a given BSTR string, using SDBM algorithm.
    /// </summary>
    /// <param name="str">The string, which is a BSTR (UCS-2 encoded),
    /// but supposed to have ASCII characters for better results.</param>
    /// <returns>The key as a 32 bits long unsigned integer.</returns>
    uint32_t HashName(XMLSTR str)
    {
#ifdef _3FD_PLATFORM_WIN32API

        uint32_t hash(0);

        wchar_t ch;
        while (ch = *str++)
            hash = static_cast<uint32_t> (towupper(ch) + (hash << 6) + (hash << 16) - hash);

        return hash;

#elif defined _3FD_PLATFORM_WINRT
        
        return static_cast<uint32_t> (str->GetHashCode());
#endif 
    }

}// end of namespace application