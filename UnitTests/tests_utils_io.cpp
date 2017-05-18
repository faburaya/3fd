#include "stdafx.h"
#include "utils_io.h"

namespace _3fd
{
namespace unit_tests
{
    /// <summary>
    /// Tests serializing arguments to text into an output file.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_UTF8_File_Test)
    {
        utils::SerializeUTF8(stdout, "serialization test ", 1, 2, 3, " check\n");
    }

}// end of namespace unit_tests
}// end of namespace _3fd
