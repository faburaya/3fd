#ifndef UTILS_IO_H // header guard
#define UTILS_IO_H

#include "exceptions.h"
#include "callstacktracer.h"

#undef min
#undef max

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <sstream>
#include <string>
#include <array>

namespace _3fd
{
namespace utils
{
    ////////////////////////////////////
    // Generic printf implementations
    ////////////////////////////////////

    /* Depending on the output and whether format string is
    of wide chars or not, the specialization of this template
    functions will choose a suitable printf-like function */
    template <typename OutType, typename CharType, typename ... Args>
    int xprintf(OutType output, const CharType *format, Args ... args)
    {
        /* this compile-time assertion is triggered when there
        is no specific implementation of printf-like function
        fitting the provided arguments */
        static_assert(0, "this generic implementation must not compile");
    }

    template <typename ... Args>
    int xprintf(FILE *file, const char *format, Args ... args)
    {
        int rc = fprintf(file, format, args ...);

        if (rc >= 0)
            return rc;
        else
            throw core::AppException<std::runtime_error>("fprintf: IO error!", strerror(errno));
    }

    template <typename ... Args>
    int xprintf(FILE *file, const wchar_t *format, Args ... args)
    {
        int rc = fwprintf(file, format, args ...);

        if (rc >= 0)
            return rc;
        else
            throw core::AppException<std::runtime_error>("fwprintf: IO error!", strerror(errno));
    }

    template <typename CharType>
    struct RawBufferInfo
    {
        CharType *data; // address of buffer data
        size_t count; // available room in amount of characters
    };

    template <typename ... Args>
    int xprintf(const RawBufferInfo<char> &buffer, const char *format, Args ... args)
    {
        int rc = snprintf(buffer.data, buffer.count, format, args ...);

        if (rc >= 0)
            return rc;
        else
            throw core::AppException<std::runtime_error>("snprintf: buffer error!", strerror(errno));
    }

    template <typename ... Args>
    int xprintf(const RawBufferInfo<wchar_t> &buffer, const wchar_t *format, Args ... args)
    {
        int rc = swprintf(buffer.data, buffer.count, format, args ...);

        if (rc >= 0)
            return rc;
        else
            throw core::AppException<std::runtime_error>("swprintf: buffer error!", strerror(errno));
    }

    /// <summary>
    /// A collection of static functions that retrieve the formats
    /// expected by printf for several built-in types.
    /// </summary>
    struct PrintFormats
    {
        static constexpr const char *utf8(char) { return "%c"; }
        static const constexpr char *utf8(void *) { return "%p"; }
        static const constexpr char *utf8(const char *) { return "%s"; }
        static const constexpr char *utf8(signed short) { return "%hd"; }
        static const constexpr char *utf8(unsigned short) { return "%hu"; }
        static const constexpr char *utf8(signed int) { return "%d"; }
        static const constexpr char *utf8(unsigned int) { return "%u"; }
        static const constexpr char *utf8(signed long) { return "%ld"; }
        static const constexpr char *utf8(unsigned long) { return "%lu"; }
        static const constexpr char *utf8(long double) { return "%G"; }

        static const constexpr char *utf8_width(void *) { return "%*p"; }
        static const constexpr char *utf8_width(const char *) { return "%*s"; }
        static const constexpr char *utf8_width(signed short) { return "%*hd"; }
        static const constexpr char *utf8_width(unsigned short) { return "%*hu"; }
        static const constexpr char *utf8_width(signed int) { return "%*d"; }
        static const constexpr char *utf8_width(unsigned int) { return "%*u"; }
        static const constexpr char *utf8_width(signed long) { return "%*ld"; }
        static const constexpr char *utf8_width(unsigned long) { return "%*lu"; }
        static const constexpr char *utf8_width(long double) { return "%*G"; }

        static const constexpr char *utf8_precision(void *) { return "%.*p"; }
        static const constexpr char *utf8_precision(const char *) { return "%.*s"; }
        static const constexpr char *utf8_precision(signed short) { return "%.*hd"; }
        static const constexpr char *utf8_precision(unsigned short) { return "%.*hu"; }
        static const constexpr char *utf8_precision(signed int) { return "%.*d"; }
        static const constexpr char *utf8_precision(unsigned int) { return "%.*u"; }
        static const constexpr char *utf8_precision(signed long) { return "%.*ld"; }
        static const constexpr char *utf8_precision(unsigned long) { return "%.*lu"; }
        static const constexpr char *utf8_precision(long double) { return "%.*G"; }

        static const constexpr char *utf8_width_precision(void *) { return "%*.*p"; }
        static const constexpr char *utf8_width_precision(const char *) { return "%*.*s"; }
        static const constexpr char *utf8_width_precision(signed short) { return "%*.*hd"; }
        static const constexpr char *utf8_width_precision(unsigned short) { return "%*.*hu"; }
        static const constexpr char *utf8_width_precision(signed int) { return "%*.*d"; }
        static const constexpr char *utf8_width_precision(unsigned int) { return "%*.*u"; }
        static const constexpr char *utf8_width_precision(signed long) { return "%*.*ld"; }
        static const constexpr char *utf8_width_precision(unsigned long) { return "%*.*lu"; }
        static const constexpr char *utf8_width_precision(long double) { return "%*.*G"; }

        static const constexpr wchar_t *ucs2(wchar_t) { return L"%c"; }
        static const constexpr wchar_t *ucs2(void *) { return L"%p"; }
        static const constexpr wchar_t *ucs2(const wchar_t *) { return L"%s"; }
        static const constexpr wchar_t *ucs2(signed short) { return L"%hd"; }
        static const constexpr wchar_t *ucs2(unsigned short) { return L"%hu"; }
        static const constexpr wchar_t *ucs2(signed int) { return L"%d"; }
        static const constexpr wchar_t *ucs2(unsigned int) { return L"%u"; }
        static const constexpr wchar_t *ucs2(signed long) { return L"%ld"; }
        static const constexpr wchar_t *ucs2(unsigned long) { return L"%lu"; }
        static const constexpr wchar_t *ucs2(long double) { return L"%G"; }

        static const constexpr wchar_t *ucs2_width(void *) { return L"%*p"; }
        static const constexpr wchar_t *ucs2_width(const wchar_t *) { return L"%*s"; }
        static const constexpr wchar_t *ucs2_width(signed short) { return L"%*hd"; }
        static const constexpr wchar_t *ucs2_width(unsigned short) { return L"%*hu"; }
        static const constexpr wchar_t *ucs2_width(signed int) { return L"%*d"; }
        static const constexpr wchar_t *ucs2_width(unsigned int) { return L"%*u"; }
        static const constexpr wchar_t *ucs2_width(signed long) { return L"%*ld"; }
        static const constexpr wchar_t *ucs2_width(unsigned long) { return L"%*lu"; }
        static const constexpr wchar_t *ucs2_width(long double) { return L"%*G"; }

        static const constexpr wchar_t *ucs2_precision(void *) { return L"%.*p"; }
        static const constexpr wchar_t *ucs2_precision(const wchar_t *) { return L"%.*s"; }
        static const constexpr wchar_t *ucs2_precision(signed short) { return L"%.*hd"; }
        static const constexpr wchar_t *ucs2_precision(unsigned short) { return L"%.*hu"; }
        static const constexpr wchar_t *ucs2_precision(signed int) { return L"%.*d"; }
        static const constexpr wchar_t *ucs2_precision(unsigned int) { return L"%.*u"; }
        static const constexpr wchar_t *ucs2_precision(signed long) { return L"%.*ld"; }
        static const constexpr wchar_t *ucs2_precision(unsigned long) { return L"%.*lu"; }
        static const constexpr wchar_t *ucs2_precision(long double) { return L"%.*G"; }

        static const constexpr wchar_t *ucs2_width_precision(void *) { return L"%*.*p"; }
        static const constexpr wchar_t *ucs2_width_precision(const wchar_t *) { return L"%*.*s"; }
        static const constexpr wchar_t *ucs2_width_precision(signed short) { return L"%*.*hd"; }
        static const constexpr wchar_t *ucs2_width_precision(unsigned short) { return L"%*.*hu"; }
        static const constexpr wchar_t *ucs2_width_precision(signed int) { return L"%*.*d"; }
        static const constexpr wchar_t *ucs2_width_precision(unsigned int) { return L"%*.*u"; }
        static const constexpr wchar_t *ucs2_width_precision(signed long) { return L"%*.*ld"; }
        static const constexpr wchar_t *ucs2_width_precision(unsigned long) { return L"%*.*lu"; }
        static const constexpr wchar_t *ucs2_width_precision(long double) { return L"%*.*G"; }
    };

    /// <summary>
    /// Wraps a generic value for serialization.
    /// </summary>
    template <typename ValType>
    class SerializableValue
    {
    private:

        ValType m_value;

        int m_width = -1;
        int m_precision = -1;

    public:

        SerializableValue(ValType value)
            : m_value(value) {}

        SerializableValue &width(int width)
        {
            m_width = width;
            return *this;
        }

        SerializableValue &precision(int precision)
        {
            m_precision = precision;
            return *this;
        }

        // Serializes the held value to UTF-8 encoded text
        template <typename OutType>
        size_t SerializeUtf8To(OutType output) const
        {
            if (m_precision < 0)
            {
                if (m_width < 0)
                    return xprintf(output, PrintFormats::utf8(m_value), m_value);
                else
                    return xprintf(output, PrintFormats::utf8_width(m_value), m_width, m_value);
            }
            else
            {
                if (m_width < 0)
                    return xprintf(output, PrintFormats::utf8_precision(m_value), m_precision, m_value);
                else
                    return xprintf(output, PrintFormats::utf8_width_precision(m_value), m_width, m_precision, m_value);
            }
        }

        // Serializes the held value to UCS-2 encoded text
        template <typename OutType>
        size_t SerializeUcs2To(OutType output) const
        {
            if (m_precision < 0)
            {
                if (m_width < 0)
                    return xprintf(output, PrintFormats::ucs2(m_value), m_value);
                else
                    return xprintf(output, PrintFormats::ucs2_width(m_value), m_width, m_value);
            }
            else
            {
                if (m_width < 0)
                    return xprintf(output, PrintFormats::ucs2_precision(m_value), m_precision, m_value);
                else
                    return xprintf(output, PrintFormats::ucs2_width_precision(m_value), m_width, m_precision, m_value);
            }
        }

        // Estimates required size for serialized string
        uint32_t EstimateStringSize() const
        {
            auto width = (m_width >= 0) ? m_width : 0;
            auto precision = (m_precision >= 0) ? m_precision : 0;
            return std::max(std::max(width, precision), 8);
        }
    };

    template <typename ValType>
    SerializableValue<ValType> WrapSerialArg(ValType value) { return SerializableValue<ValType>(value); }

    SerializableValue<const char *> WrapSerialArg(const std::string &value);

    SerializableValue<const wchar_t *> WrapSerialArg(const std::wstring &value);

    template <typename ValType>
    const SerializableValue<ValType> &WrapSerialArg(const SerializableValue<ValType> &wsval) { return wsval; }


    //////////////////////////////
    // Serialization Helpers
    //////////////////////////////

    size_t _estimate_string_size() { return 0; }

    template <typename FirstArgVType, typename ... Args>
    size_t _estimate_string_size(FirstArgVType &&firstArg, Args ... args)
    {
        return WrapSerialArg(std::forward(firstArg)).EstimateStringSize() + _estimate_string_size(args ...);
    }

    template <typename ... Args>
    size_t _estimate_string_size(Args ... args)
    {
        return _estimate_string_size(args ...);
    }

    template <typename OutType, typename FirstArgVType, typename ... Args>
    size_t _serialize_utf8_impl(OutType output, FirstArgVType &&firstArg, Args ... args)
    {
        /* this compile-time assertion is triggered when there
        is no specific implementation for output of printf-like
        function fitting the provided arguments */
        static_assert(0, "this generic implementation must not compile");
    }

    template <typename FirstArgVType, typename ... Args>
    size_t _serialize_utf8_impl(FILE *file, FirstArgVType &&firstArg, Args ... args)
    {
        return WrapSerialArg(firstArg).SerializeUtf8To(file) + _serialize_utf8_impl(file, args ...);
    }

    template <typename FirstArgVType, typename ... Args>
    size_t _serialize_utf8_impl(const RawBufferInfo<char> &buffer, FirstArgVType &&firstArg, Args ... args)
    {
        auto wcount = WrapSerialArg(firstArg).SerializeUtf8To(buffer);

        if (wcount < buffer.count)
            return wcount + _serialize_utf8_impl(RawBufferInfo<char>{ buffer.data + wcount, buffer.count - wcount }, args ...);
        else
            return wcount + 1; // needs more room
    }

    template <typename OutType>
    size_t _serialize_utf8_impl(OutType output)
    {
        return 0; // NO-OP
    }

    /// <summary>
    /// Serializes to an output file the argument values as UTF-8 encoded text.
    /// </summary>
    /// <param name="file">The output file.</param>
    /// <param name="...args">The values to serialize, all wrapped in <see cref="SerializableValue" /> objects.</param>
    /// <returns>The length of text written into the output file.</returns>
    template <typename ... Args>
    size_t SerializeUTF8(FILE *file, Args ... args)
    {
        CALL_STACK_TRACE;
        return _serialize_utf8_impl(file, args ...);
    }

    /// <summary>
    /// Serializes to a buffer the argument values as UTF-8 encoded text.
    /// </summary>
    /// <param name="buffer">The output buffer.</param>
    /// <param name="...args">The values to serialize, all wrapped in <see cref="SerializableValue" /> objects.</param>
    /// <returns>The length of text written into the buffer.</returns>
    template <size_t N, typename ... Args>
    size_t SerializeUTF8(std::array<char, N> &buffer, Args ... args)
    {
        CALL_STACK_TRACE;

        auto wcount = _serialize_utf8_impl(RawBufferInfo<char>{ buffer.data(), buffer.size() }, args ...);

        if (wcount < buffer.size())
            return wcount;
        else
            throw core::AppException<std::logic_error>("Failed to serialize arguments: buffer is too short!");
    }

    /// <summary>
    /// Serializes to a string the argument values as UTF-8 encoded text.
    /// </summary>
    /// <param name="out">The output string.</param>
    /// <param name="...args">The values to serialize, all wrapped in <see cref="SerializableValue" /> objects.</param>
    /// <returns>The length of text written into the string.</returns>
    template <typename ... Args>
    size_t SerializeUTF8(std::string &out, Args ... args)
    {
        CALL_STACK_TRACE;

        try
        {
            auto estReqSize = _estimate_string_size(args ...);
            if (out.size() < estReqSize)
                out.resize(estReqSize);

            while (true)
            {
                auto wcount = _serialize_utf8_impl(RawBufferInfo<char>{ out.data(), out.size() }, args ...);

                // string buffer was big enough?
                if (out.size() > wcount)
                {
                    out.resize(wcount + 1);
                    return wcount;
                }
                
                // make more room:
                out.resize(
                    (out.size() * 2 > wcount) ? out.size() * 2 : out.size() + wcount
                );
            }
        }
        catch (core::IAppException &)
        {
            throw; // just forward already prepared application exceptions
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Failed to serialize arguments: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

    template <typename OutType, typename FirstArgVType, typename ... Args>
    size_t _serialize_ucs2_impl(OutType output, FirstArgVType &&firstArg, Args ... args)
    {
        /* this compile-time assertion is triggered when there
        is no specific implementation for output of printf-like
        function fitting the provided arguments */
        static_assert(0, "this generic implementation must not compile");
    }

    template <typename FirstArgVType, typename ... Args>
    size_t _serialize_ucs2_impl(FILE *file, FirstArgVType &&firstArg, Args ... args)
    {
        return WrapSerialArg(firstArg).SerializeUcs2To(file) + _serialize_ucs2_impl(file, args ...);
    }

    template <typename FirstArgVType, typename ... Args>
    size_t _serialize_ucs2_impl(const RawBufferInfo<wchar_t> &buffer, FirstArgVType &&firstArg, Args ... args)
    {
        auto wcount = WrapSerialArg(firstArg).SerializeUcs2To(buffer);

        if (wcount < buffer.count)
            return wcount + _serialize_ucs2_impl(RawBufferInfo<wchar_t>{ buffer.data + wcount, buffer.count - wcount }, args ...);
        else
            return wcount + 1; // needs more room
    }

    template <typename OutType>
    size_t _serialize_ucs2_impl(OutType output)
    {
        return 0; // NO-OP
    }

    /// <summary>
    /// Serializes to an output file the argument values as UCS-2 encoded text.
    /// </summary>
    /// <param name="file">The output file.</param>
    /// <param name="...args">The values to serialize, all wrapped in <see cref="SerializableValue" /> objects.</param>
    /// <returns>The length of text written into the output file.</returns>
    template <typename ... Args>
    size_t SerializeUCS2(FILE *file, Args ... args)
    {
        CALL_STACK_TRACE;
        return _serialize_ucs2_impl(file, args ...);
    }

    /// <summary>
    /// Serializes to a buffer the argument values as UCS-2 encoded text.
    /// </summary>
    /// <param name="file">The output buffer.</param>
    /// <param name="...args">The values to serialize, all wrapped in <see cref="SerializableValue" /> objects.</param>
    /// <returns>The length of text written into the buffer.</returns>
    template <size_t N, typename ... Args>
    size_t SerializeUTF8(std::array<wchar_t, N> &buffer, Args ... args)
    {
        CALL_STACK_TRACE;

        auto wcount = _serialize_ucs2_impl(RawBufferInfo<wchar_t>{ buffer.data(), buffer.size() }, args ...);

        if (wcount < buffer.size())
            return wcount;
        else
            throw core::AppException<std::logic_error>("Failed to serialize: buffer is too short!");
    }

    /// <summary>
    /// Serializes to a string the argument values as UCS-2 encoded text.
    /// </summary>
    /// <param name="out">The output string.</param>
    /// <param name="...args">The values to serialize, all wrapped in <see cref="SerializableValue" /> objects.</param>
    /// <returns>The length of text written into the string.</returns>
    template <typename ... Args>
    size_t SerializeUCS2(std::wstring &out, Args ... args)
    {
        CALL_STACK_TRACE;

        try
        {
            auto estReqSize = _estimate_string_size(args ...);
            if (out.size() < estReqSize)
                out.resize(estReqSize);

            while (true)
            {
                auto wcount = _serialize_ucs2_impl(RawBufferInfo<wchar_t>{ out.data(), out.size() }, args ...);

                // string buffer was big enough?
                if (out.size() > wcount)
                {
                    out.resize(wcount + 1);
                    return wcount;
                }

                // make more room:
                out.resize(
                    (out.size() * 2 > wcount) ? out.size() * 2 : out.size() + wcount
                );
            }
        }
        catch (core::IAppException &)
        {
            throw; // just forward already prepared application exceptions
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Failed to serialize arguments: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
