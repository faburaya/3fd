#include "stdafx.h"
#include "WicUtilities.h"

namespace application
{
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
    uint32_t HashName(BSTR str)
    {
        uint32_t hash(0);

        wchar_t ch;
        while (ch = *str++)
            hash = static_cast<uint32_t> (towupper(ch) + (hash << 6) + (hash << 16) - hash);

        return hash;
    }

}// end of namespace application