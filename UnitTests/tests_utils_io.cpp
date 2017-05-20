#include "stdafx.h"
#include "utils_io.h"
#include <iostream>
#include <sstream>
#include <codecvt>
#include <ctime>

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
        utils::SerializeTo<char>(stdout, "serialize address: ", &notUsed, '\n');

        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize  UTF-8: ", format("foobar").width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize  UTF-8: ", format(std::string("foobar")).width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize  UCS-2: ", format(L"foobar").width(8), '\n'));
        EXPECT_EQ(27, utils::SerializeTo<char>(stdout, "serialize  UCS-2: ", format(std::wstring(L"foobar")).width(8), '\n'));
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
    /// Tests serializing arguments to wide-char text into an output file.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_WideChar_File_Test)
    {
        EXPECT_EQ(32, utils::SerializeTo<wchar_t>(stdout, L"serialization test ", 1, 2, 3, L"... check\n"));

        int notUsed;
        utils::SerializeTo<wchar_t>(stdout, L"serialize address: ", &notUsed, L'\n');

        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize  UTF-8: ", format("foobar").width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize  UTF-8: ", format(std::string("foobar")).width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize  UCS-2: ", format(L"foobar").width(8), L'\n'));
        EXPECT_EQ(27, utils::SerializeTo<wchar_t>(stdout, L"serialize  UCS-2: ", format(std::wstring(L"foobar")).width(8), L'\n'));
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
            "), string(UTF-8), ",
            L"string(wide)");

        auto expsstr = "serialization test: int32(42), float(0.42), string(UTF-8), string(wide)";
        EXPECT_STREQ(expsstr, buffer.data());
        EXPECT_EQ(strlen(expsstr), pcount);

        pcount = utils::SerializeTo(buffer,
            "serialization test: int32(", (int32_t)4242,
            "), float(", format(0.4242F).precision(4),
            "), string(UTF-8 UTF-8), ",
            L"string(wide wide)");

        expsstr = "serialization test: int32(4242), float(0.4242), string(UTF-8 UTF-8), string(wide wide)";
        EXPECT_STREQ(expsstr, buffer.data());
        EXPECT_EQ(strlen(expsstr), pcount);
    }

    /// <summary>
    /// Tests serializing arguments to wide-char text into a statically sized output buffer.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_WideChar_Array_Test)
    {
        size_t pcount;
        std::array<wchar_t, 100> buffer;

        pcount = utils::SerializeTo(buffer,
            L"serialization test: int32(", (int32_t)42,
            L"), float(", format(0.42F).precision(2),
            L"), string(wide), ",
            "string(UTF-8)");

        auto expsstr = L"serialization test: int32(42), float(0.42), string(wide), string(UTF-8)";
        EXPECT_STREQ(expsstr, buffer.data());
        EXPECT_EQ(wcslen(expsstr), pcount);

        pcount = utils::SerializeTo(buffer,
            L"serialization test: int32(", (int32_t)4242,
            L"), float(", format(0.4242F).precision(4),
            L"), string(wide wide), ",
            "string(UTF-8 UTF-8)");

        expsstr = L"serialization test: int32(4242), float(0.4242), string(wide wide), string(UTF-8 UTF-8)";
        EXPECT_STREQ(expsstr, buffer.data());
        EXPECT_EQ(wcslen(expsstr), pcount);
    }

    /// <summary>
    /// Tests serializing arguments to UTF-8 text into an output string object.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_UTF8_String_Test)
    {
        try
        {
            size_t pcount;
            std::string out;

            pcount = utils::SerializeTo(out,
                "serialization test: int32(", (int32_t)42,
                "), float(", format(0.42F).precision(2),
                "), string(UTF-8), ",
                L"string(wide)");

            auto expsstr = "serialization test: int32(42), float(0.42), string(UTF-8), string(wide)";
            EXPECT_EQ(expsstr, out);
            EXPECT_EQ(out.length(), pcount);

            pcount = utils::SerializeTo(out,
                "serialization test: int32(", (int32_t)4242,
                "), float(", format(0.4242F).precision(4),
                "), string(UTF-8 UTF-8), ",
                L"string(wide wide)");

            expsstr = "serialization test: int32(4242), float(0.4242), string(UTF-8 UTF-8), string(wide wide)";
            EXPECT_EQ(expsstr, out);
            EXPECT_EQ(out.length(), pcount);
        }
        catch (core::IAppException &ex)
        {
            std::cerr << ex.ToString() << std::endl;
            FAIL();
        }
    }

    /// <summary>
    /// Tests serializing arguments wide-char text into an output string.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_WideChar_String_Test)
    {
        try
        {
            size_t pcount;
            std::wstring out;

            pcount = utils::SerializeTo(out,
                L"serialization test: int32(", (int32_t)42,
                L"), float(", format(0.42F).precision(2),
                L"), string(wide), ",
                "string(UTF-8)");

            auto expsstr = L"serialization test: int32(42), float(0.42), string(wide), string(UTF-8)";
            EXPECT_EQ(expsstr, out);
            EXPECT_EQ(out.length(), pcount);

            pcount = utils::SerializeTo(out,
                L"serialization test: int32(", (int32_t)4242,
                L"), float(", format(0.4242F).precision(4),
                L"), string(wide wide), ",
                "string(UTF-8 UTF-8)");

            expsstr = L"serialization test: int32(4242), float(0.4242), string(wide wide), string(UTF-8 UTF-8)";
            EXPECT_EQ(expsstr, out);
            EXPECT_EQ(out.length(), pcount);
        }
        catch (core::IAppException &ex)
        {
            std::cerr << ex.ToString() << std::endl;
            FAIL();
        }
    }

    /// <summary>
    /// Helps measuring elapsed time for the interval of execution inside a scope.
    /// </summary>
    class ScopedTimer
    {
    private:

        clock_t m_startTimeTicks, m_endTimeTicks;

    public:

        ScopedTimer()
            : m_startTimeTicks(clock()) {}

        ~ScopedTimer()
        {
            m_endTimeTicks = clock();

            std::cout << std::setprecision(4)
                      << 1000.0 * (m_endTimeTicks - m_startTimeTicks) / CLOCKS_PER_SEC
                      << " ms";
        }
    };

    /// <summary>
    /// Tests serialization speed to encode UTF-8 text into a statically sized output buffer.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_UTF8_Array_Speed_Test)
    {
        size_t nChars(0);
        const int nIterations(32768);
        std::array<char, 100> buffer;

        {// framework serialization:
            std::cout << "framework: ";

            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                nChars += utils::SerializeTo(buffer,
                    "serialization test: ",
                    (int32_t)42, "; ",
                    format(0.42F).precision(2), "; ",
                    "this is UTF-8 text; ",
                    L"this is wide-char text");
            }
        }
        
        std::cout << " (serialized " << nChars * sizeof(char) << " bytes)" << std::endl;

        {// sprintf:
            std::cout << "  sprintf: ";

            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                sprintf(buffer.data(),
                    "initialization test: %d; %.2G; this is UTF-8 text; %ls",
                    42, 0.42F, L"this is wide-char text");
            }
        }

        std::cout << std::endl;

        {// sstream:
            std::cout << "  sstream: ";

            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            std::ostringstream oss;

            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                oss << "initialization test: " << 42 << "; "
                    << std::setprecision(2) << 0.42F
                    << "; this is UTF-8 text; "
                    << transcoder.to_bytes(L"this is wide-char text");

                oss.str("");
            }
        }

        std::cout << std::endl;
    }

    /// <summary>
    /// Tests serialization speed to encode wide-char text into a statically sized output buffer.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_WideChar_Array_Speed_Test)
    {
        size_t nChars(0);
        const int nIterations(32768);
        std::array<wchar_t, 100> buffer;

        {// framework serialization:
            std::cout << "framework: ";

            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                nChars += utils::SerializeTo(buffer,
                    L"serialization test: ",
                    (int32_t)42, "; ",
                    format(0.42F).precision(2), "; ",
                    L"this is wide-char text; ",
                    "this is UTF-8 text");
            }
        }

        std::cout << " (serialized " << nChars * sizeof(wchar_t) << " bytes)" << std::endl;

        {// sprintf:
            std::cout << "  sprintf: ";

            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                swprintf(buffer.data(),
                    L"initialization test: %d; %.2G; this is wide-char text; %hs",
                    42, 0.42F, "this is UTF-8 text");
            }
        }

        std::cout << std::endl;

        {// sstream:
            std::cout << "  sstream: ";

            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            std::wostringstream woss;

            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                woss << L"initialization test: " << 42 << "; "
                     << std::setprecision(2) << 0.42F
                     << L"; this is wide-char text; "
                     << transcoder.from_bytes("this is UTF-8 text");

                woss.str(L"");
            }
        }

        std::cout << std::endl;
    }

    /// <summary>
    /// Tests serialization speed to encode UTF-8 text into an output string.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_UTF8_String_Speed_Test)
    {
        size_t nChars(0);
        const int nIterations(32768);

        {// framework serialization:
            std::cout << "framework: ";

            std::string out;
            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                nChars += utils::SerializeTo(out,
                    "serialization test: ",
                    (int32_t)42, "; ",
                    format(0.42F).precision(2), "; ",
                    "this is UTF-8 text; ",
                    L"this is wide-char text");
            }
        }

        std::cout << " (serialized " << nChars * sizeof(char) << " bytes)" << std::endl;

        {// sprintf:
            std::cout << "  sprintf: ";

            std::array<char, 100> buffer;
            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                sprintf(buffer.data(),
                    "initialization test: %d; %.2G; this is UTF-8 text; %ls",
                    42, 0.42F, L"this is wide-char text");
            }
        }

        std::cout << std::endl;

        {// sstream:
            std::cout << "  sstream: ";

            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            std::ostringstream oss;

            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                oss << "initialization test: " << 42 << "; "
                    << std::setprecision(2) << 0.42F
                    << "; this is UTF-8 text; "
                    << transcoder.to_bytes(L"this is wide-char text");

                oss.str("");
            }
        }

        std::cout << std::endl;
    }

    //// <summary>
    /// Tests serialization speed to encode wide-char text into an output string.
    /// </summary>
    TEST(Framework_Utils_TestCase, Serialization_WideChar_String_Speed_Test)
    {
        size_t nChars(0);
        const int nIterations(32768);

        {// framework serialization:
            std::cout << "framework: ";

            std::wstring out;
            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                nChars += utils::SerializeTo(out,
                    L"serialization test: ",
                    (int32_t)42, "; ",
                    format(0.42F).precision(2), "; ",
                    L"this is wide-char text; ",
                    "this is UTF-8 text");
            }
        }

        std::cout << " (serialized " << nChars * sizeof(wchar_t) << " bytes)" << std::endl;

        {// sprintf:
            std::cout << "  sprintf: ";

            std::array<wchar_t, 100> buffer;
            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                swprintf(buffer.data(),
                    L"initialization test: %d; %.2G; this is wide-char text; %hs",
                    42, 0.42F, "this is UTF-8 text");
            }
        }

        std::cout << std::endl;

        {// sstream:
            std::cout << "  sstream: ";

            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            std::wostringstream woss;

            ScopedTimer timer;

            for (int i = 0; i < nIterations; ++i)
            {
                woss << L"initialization test: " << 42 << "; "
                    << std::setprecision(2) << 0.42F
                    << L"; this is wide-char text; "
                    << transcoder.from_bytes("this is UTF-8 text");

                woss.str(L"");
            }
        }

        std::cout << std::endl;
    }

}// end of namespace unit_tests
}// end of namespace _3fd
