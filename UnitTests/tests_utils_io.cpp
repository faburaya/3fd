#include "stdafx.h"
#include "utils_io.h"

#define format utils::FormatArg

namespace _3fd
{
namespace unit_tests
{
    /// <summary>
    /// Tests serializing arguments to UTF-8 text into an output file.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_UTF8_File_Test)
    {
        EXPECT_EQ(32, utils::SerializeTo<char>(stdout, "serialization test ", 1, 2, 3, "... check\n"));
        
        int notUsed;
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize address: ", &notUsed, '\n'));

        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize string: ", format("foobar").width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize string: ", format(std::string("foobar")).width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize uint16: ", format((uint16_t)42).width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize uint32: ", format((uint32_t)4242).width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize uint64: ", format((uint64_t)424242).width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize  int16: ", format((int16_t)42).width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize  int32: ", format((int32_t)4242).width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize  int64: ", format((int64_t)424242).width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize  float: ", format(42.42F).width(8).precision(4), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize double: ", format(42.4242).width(8).precision(4), '\n'));
    }

    /// <summary>
    /// Tests serializing arguments to UCS-2 text into an output file.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_UCS2_File_Test)
    {
        EXPECT_EQ(32, utils::SerializeTo<wchar_t>(stdout, L"serialization test ", 1, 2, 3, L"... check\n"));

        int notUsed;
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize address: ", &notUsed, L'\n'));

        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize string: ", format(L"foobar").width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize string: ", format(std::wstring(L"foobar")).width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize uint16: ", format((uint16_t)42).width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize uint32: ", format((uint32_t)4242).width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize uint64: ", format((uint64_t)424242).width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize  int16: ", format((int16_t)42).width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize  int32: ", format((int32_t)4242).width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize  int64: ", format((int64_t)424242).width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize  float: ", format(42.42F).width(8).precision(4), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize double: ", format(42.4242).width(8).precision(4), L'\n'));
    }

    /// <summary>
    /// Tests serializing arguments to UTF-8 text into a statically sized output buffer.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_UTF8_Array_Test)
    {
        size_t pcount;
        std::array<char, 100> buffer;

        pcount = utils::SerializeTo(buffer,
            "serialization test: int32(", (int32_t)42,
            "), float(", format(0.42F).precision(2),
            "), string(UTF-8)");

        auto expsstr = "serialization test: int32(42), float(0.42), string(UTF-8)";
        EXPECT_STREQ(expsstr, buffer.data());
        EXPECT_EQ(strlen(expsstr), pcount);

        pcount = utils::SerializeTo(buffer,
            "serialization test: int32(", (int32_t)4242,
            "), float(", format(0.4242F).precision(4),
            "), string(UTF-8 UTF-8)");

        expsstr = "serialization test: int32(4242), float(0.4242), string(UTF-8 UTF-8)";
        EXPECT_STREQ(expsstr, buffer.data());
        EXPECT_EQ(strlen(expsstr), pcount);
    }

    /// <summary>
    /// Tests serializing arguments to UCS-2 text into a statically sized output buffer.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_UCS2_Array_Test)
    {
        size_t pcount;
        std::array<wchar_t, 100> buffer;

        pcount = utils::SerializeTo(buffer,
            L"serialization test: int32(", (int32_t)42,
            L"), float(", format(0.42F).precision(2),
            L"), string(wide)");

        auto expsstr = L"serialization test: int32(42), float(0.42), string(wide)";
        EXPECT_STREQ(expsstr, buffer.data());
        EXPECT_EQ(wcslen(expsstr), pcount);

        pcount = utils::SerializeTo(buffer,
            L"serialization test: int32(", (int32_t)4242,
            L"), float(", format(0.4242F).precision(4),
            L"), string(wide wide)");

        expsstr = L"serialization test: int32(4242), float(0.4242), string(wide wide)";
        EXPECT_STREQ(expsstr, buffer.data());
        EXPECT_EQ(wcslen(expsstr), pcount);
    }

    /// <summary>
    /// Tests serializing arguments to UTF-8 text into an output string object.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_UTF8_String_Test)
    {
        size_t pcount;
        std::string out;

        pcount = utils::SerializeTo(out,
            "serialization test: int32(", (int32_t)42,
            "), float(", format(0.42F).precision(2),
            "), string(UTF-8)");

        auto expsstr = "serialization test: int32(42), float(0.42), string(foobar)";
        EXPECT_EQ(expsstr, out);
        EXPECT_EQ(out.length(), pcount);

        pcount = utils::SerializeTo(out,
            "serialization test: int32(", (int32_t)4242,
            "), float(", format(0.4242F).precision(4),
            "), string(UTF-8 UTF-8)");

        expsstr = "serialization test: int32(4242), float(0.4242), string(foobar foobar)";
        EXPECT_EQ(expsstr, out);
        EXPECT_EQ(out.length(), pcount);
    }

    /// <summary>
    /// Tests serializing arguments to UCS-2 text into an output string.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_UCS2_String_Test)
    {
        size_t pcount;
        std::wstring out;

        pcount = utils::SerializeTo(out,
            L"serialization test: int32(", (int32_t)42,
            L"), float(", format(0.42F).precision(2),
            L"), string(wide)");

        auto expsstr = L"serialization test: int32(42), float(0.42), string(wide)";
        EXPECT_EQ(expsstr, out);
        EXPECT_EQ(out.length(), pcount);

        pcount = utils::SerializeTo(out,
            L"serialization test: int32(", (int32_t)4242,
            L"), float(", format(0.4242F).precision(4),
            L"), string(wide wide)");

        expsstr = L"serialization test: int32(4242), float(0.4242), string(wide wide)";
        EXPECT_EQ(expsstr, out);
        EXPECT_EQ(out.length(), pcount);
    }

}// end of namespace unit_tests
}// end of namespace _3fd
