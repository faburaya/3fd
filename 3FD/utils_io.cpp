#include "stdafx.h"
#include "utils_io.h"
#include <cerrno>

namespace _3fd
{
namespace utils
{
    ///////////////////////////////////////
    // Specialization of serializations
    ///////////////////////////////////////

    SerializableValue<const char *> FormatArg(const std::string &value)
    {
        return SerializableValue<const char *>(value.c_str());
    }

    SerializableValue<const wchar_t *> FormatArg(const std::wstring &value)
    {
        return SerializableValue<const wchar_t *>(value.c_str());
    }

    size_t _estimate_string_size()
    {
        return 0;
    }

}// end of namespace utils
}// end of namespace _3fd