//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include "utils_string.h"
#include <codecvt>
#include <locale>

namespace _3fd
{
namespace utils
{

    ////////////////////////////////////////////////
    // Unicode Conversion
    ////////////////////////////////////////////////

    std::wstring to_ucs2(std::string_view input)
    {
        if (!input.empty())
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            return transcoder.from_bytes(input.data(), input.data() + input.length());
        }

        return std::wstring();
    }

    std::string to_utf8(std::wstring_view input)
    {
        if (!input.empty())
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            return transcoder.to_bytes(input.data(), input.data() + input.length());
        }

        return std::string();
    }

}// end of namespace utils
}// end of namespace _3fd
