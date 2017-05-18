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

    SerializableValue<const char *> WrapSerialArg(const std::string &value)
    {
        return SerializableValue<const char *>(value.c_str());
    }

    SerializableValue<const wchar_t *> WrapSerialArg(const std::wstring &value)
    {
        return SerializableValue<const wchar_t *>(value.c_str());
    }

}// end of namespace utils
}// end of namespace _3fd