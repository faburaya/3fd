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

    size_t _estimate_string_size()
    {
        return 0;
    }

}// end of namespace utils
}// end of namespace _3fd