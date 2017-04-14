#ifndef WICUTILITIES_H // header guard
#define WICUTILITIES_H

#include "3FD\base.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include <MsXml6.h>
#include <comutil.h>

#undef min
#undef max

#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <limits>

#define CHECK(COM_CALL_EXPR) if ((hr = (COM_CALL_EXPR)) < 0) { WWAPI::RaiseHResultException(hr, "Unexpected error in COM interface call", #COM_CALL_EXPR); }

namespace application
{
    using namespace _3fd;
    using namespace _3fd::core;

    using namespace Microsoft::WRL;

    BSTR ExtractBstrFrom(_variant_t &wrappedVar);

    /// <summary>
    /// Gets the value from a VARIANT.
    /// </summary>
    /// <param name="variant">The variant object.</param>
    /// <returns>A reference to the value held by the VARIANT.</returns>
    template <typename ValType>
    ValType GetValueFrom(_variant_t &variant)
    {
        static_cast<false>; // causes compilation failure in case this generic implementation gets compiled
        return variant;
    }

    template <> BSTR GetValueFrom<BSTR>(_variant_t &variant);
    template <> bool GetValueFrom<bool>(_variant_t &variant);
    template <> uint16_t GetValueFrom<uint16_t>(_variant_t &variant);
    template <> uint32_t GetValueFrom<uint32_t>(_variant_t &variant);
    template <> int16_t GetValueFrom<int16_t>(_variant_t &variant);
    template <> int32_t GetValueFrom<int32_t>(_variant_t &variant);

    /// <summary>
    /// Gets the XML attribute value.
    /// </summary>
    /// <param name="attributes">The attributes.</param>
    /// <param name="attrName">Name of the attribute.</param>
    /// <param name="value">A reference to the variable to receive the value.</param>
    template <typename ValType>
    void GetAttributeValue(IXMLDOMNamedNodeMap *attributes,
                           BSTR attrName,
                           VARTYPE typeConv,
                           ValType &value)
    {
        CALL_STACK_TRACE;

        HRESULT hr;
        _variant_t variant;
        ComPtr<IXMLDOMNode> attrNode;
        CHECK(attributes->getNamedItem(attrName, attrNode.GetAddressOf()));
        CHECK(attrNode->get_nodeValue(variant.GetAddress()));
        _ASSERTE(variant.vt == VT_BSTR);
        value = GetValueFrom<ValType>(variant);
    }

    /// <summary>
    /// Gets the XML attribute value as a hash.
    /// </summary>
    /// <param name="attributes">The attributes.</param>
    /// <param name="attrName">Name of the attribute.</param>
    /// <param name="typeConv">The type conversion to perform.</param>
    /// <param name="value">A reference to the variable to receive the value.</param>
    /// <returns>The originally parsed BSTR value.</returns>
    template <typename ValHashType>
    _bstr_t GetAttributeValueHash(IXMLDOMNamedNodeMap *attributes,
                                  BSTR attrName,
                                  ValHashType &hash)
    {
        BSTR value;
        GetAttributeValue(attributes, attrName, VT_BSTR, value);
        hash = HashName(value);
        return _bstr_t(value, false);
    }

    uint32_t HashGuid(const GUID &guid);

    uint64_t MakeKey(uint32_t low, uint32_t high);

    uint64_t MakeKey(const GUID &srcFormatGuid, const GUID &destFormatGuid);

    uint32_t HashName(BSTR str);
    
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

}// end of namespace application

#endif // end of header guard
