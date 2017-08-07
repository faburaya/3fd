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

#ifdef __linux__
#   include <wchar.h>
#endif

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
    #ifdef _MSC_VER
        /* this compile-time assertion is triggered when there
        is no specific implementation of printf-like function
        fitting the provided arguments */
        static_assert(0, "this generic implementation must not compile");
    #else
        // NOT IMPLEMENTED:
        assert(false);
        throw core::AppException<std::runtime_error>("xprintf: overload not implemented!");
    #endif
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
            throw core::AppException<std::runtime_error>("snprintf: encoding error!", strerror(errno));
    }

    template <typename ... Args>
    int xprintf(const RawBufferInfo<wchar_t> &buffer, const wchar_t *format, Args ... args)
    {
#   ifdef _WIN32
        _set_errno(0);
#   else
        errno = 0;
#   endif
        int rc = swprintf(buffer.data, buffer.count, format, args ...);

        if (rc >= 0)
            return rc;
        else if (errno == 0)
            return static_cast<int> (2 * buffer.count);
        else
            throw core::AppException<std::runtime_error>("swprintf: encoding error!", strerror(errno));
    }

    template <typename CharType> struct PrintFormats {};

    /// <summary>
    /// A collection of static functions that retrieve the formats
    /// expected by printf for several built-in types.
    /// </summary>
    template<> struct PrintFormats<char>
    {
         static constexpr const char *place_holder(char) { return "%c"; }
         static constexpr const char *place_holder(wchar_t) { return "%lc"; }
         static constexpr const char *place_holder(void *) { return "%p"; }
         static constexpr const char *place_holder(const char *) { return "%s"; }
         static constexpr const char *place_holder(const wchar_t *) { return "%ls"; }
         static constexpr const char *place_holder(signed short) { return "%hd"; }
         static constexpr const char *place_holder(unsigned short) { return "%hu"; }
         static constexpr const char *place_holder(signed int) { return "%d"; }
         static constexpr const char *place_holder(unsigned int) { return "%u"; }
         static constexpr const char *place_holder(signed long) { return "%ld"; }
         static constexpr const char *place_holder(unsigned long) { return "%lu"; }
         static constexpr const char *place_holder(signed long long) { return "%lld"; }
         static constexpr const char *place_holder(unsigned long long) { return "%llu"; }
         static constexpr const char *place_holder(double) { return "%G"; }
         static constexpr const char *place_holder(long double) { return "%lG"; }

         static constexpr const char *place_holder_width(void *) { return "%*p"; }
         static constexpr const char *place_holder_width(const char *) { return "%*s"; }
         static constexpr const char *place_holder_width(const wchar_t *) { return "%*ls"; }
         static constexpr const char *place_holder_width(signed short) { return "%*hd"; }
         static constexpr const char *place_holder_width(unsigned short) { return "%*hu"; }
         static constexpr const char *place_holder_width(signed int) { return "%*d"; }
         static constexpr const char *place_holder_width(unsigned int) { return "%*u"; }
         static constexpr const char *place_holder_width(signed long) { return "%*ld"; }
         static constexpr const char *place_holder_width(unsigned long) { return "%*lu"; }
         static constexpr const char *place_holder_width(signed long long) { return "%*lld"; }
         static constexpr const char *place_holder_width(unsigned long long) { return "%*llu"; }
         static constexpr const char *place_holder_width(double) { return "%*G"; }
         static constexpr const char *place_holder_width(long double) { return "%*lG"; }

         static constexpr const char *place_holder_precision(void *) { return "%.*p"; }
         static constexpr const char *place_holder_precision(const char *) { return "%.*s"; }
         static constexpr const char *place_holder_precision(const wchar_t *) { return "%.*ls"; }
         static constexpr const char *place_holder_precision(signed short) { return "%.*hd"; }
         static constexpr const char *place_holder_precision(unsigned short) { return "%.*hu"; }
         static constexpr const char *place_holder_precision(signed int) { return "%.*d"; }
         static constexpr const char *place_holder_precision(unsigned int) { return "%.*u"; }
         static constexpr const char *place_holder_precision(signed long) { return "%.*ld"; }
         static constexpr const char *place_holder_precision(unsigned long) { return "%.*lu"; }
         static constexpr const char *place_holder_precision(signed long long) { return "%.*lld"; }
         static constexpr const char *place_holder_precision(unsigned long long) { return "%.*llu"; }
         static constexpr const char *place_holder_precision(double) { return "%.*G"; }
         static constexpr const char *place_holder_precision(long double) { return "%.*lG"; }

         static constexpr const char *place_holder_width_precision(void *) { return "%*.*p"; }
         static constexpr const char *place_holder_width_precision(const char *) { return "%*.*s"; }
         static constexpr const char *place_holder_width_precision(const wchar_t *) { return "%*.*ls"; }
         static constexpr const char *place_holder_width_precision(signed short) { return "%*.*hd"; }
         static constexpr const char *place_holder_width_precision(unsigned short) { return "%*.*hu"; }
         static constexpr const char *place_holder_width_precision(signed int) { return "%*.*d"; }
         static constexpr const char *place_holder_width_precision(unsigned int) { return "%*.*u"; }
         static constexpr const char *place_holder_width_precision(signed long) { return "%*.*ld"; }
         static constexpr const char *place_holder_width_precision(unsigned long) { return "%*.*lu"; }
         static constexpr const char *place_holder_width_precision(signed long long) { return "%*.*lld"; }
         static constexpr const char *place_holder_width_precision(unsigned long long) { return "%*.*llu"; }
         static constexpr const char *place_holder_width_precision(double) { return "%*.*G"; }
         static constexpr const char *place_holder_width_precision(long double) { return "%*.*lG"; }
    };

    /// <summary>
    /// A collection of static functions that retrieve the formats
    /// expected by wprintf for several built-in types.
    /// </summary>
    template<> struct PrintFormats<wchar_t>
    {
         static constexpr const wchar_t *place_holder(wchar_t) { return L"%c"; }
         static constexpr const wchar_t *place_holder(void *) { return L"%p"; }
    #ifdef _MSC_VER
         static constexpr const wchar_t *place_holder(char) { return L"%hc"; }
         static constexpr const wchar_t *place_holder(const char *) { return L"%hs"; }
         static constexpr const wchar_t *place_holder(const wchar_t *) { return L"%s"; }
    #else
         static constexpr const wchar_t *place_holder(char) { return L"%c"; }
         static constexpr const wchar_t *place_holder(const char *) { return L"%s"; }
         static constexpr const wchar_t *place_holder(const wchar_t *) { return L"%ls"; }
    #endif
         static constexpr const wchar_t *place_holder(signed short) { return L"%hd"; }
         static constexpr const wchar_t *place_holder(unsigned short) { return L"%hu"; }
         static constexpr const wchar_t *place_holder(signed int) { return L"%d"; }
         static constexpr const wchar_t *place_holder(unsigned int) { return L"%u"; }
         static constexpr const wchar_t *place_holder(signed long) { return L"%ld"; }
         static constexpr const wchar_t *place_holder(unsigned long) { return L"%lu"; }
         static constexpr const wchar_t *place_holder(signed long long) { return L"%lld"; }
         static constexpr const wchar_t *place_holder(unsigned long long) { return L"%llu"; }
         static constexpr const wchar_t *place_holder(double) { return L"%G"; }
         static constexpr const wchar_t *place_holder(long double) { return L"%lG"; }

         static constexpr const wchar_t *place_holder_width(void *) { return L"%*p"; }
    #ifdef _MSC_VER
         static constexpr const wchar_t *place_holder_width(const char *) { return L"%*hs"; }
         static constexpr const wchar_t *place_holder_width(const wchar_t *) { return L"%*s"; }
    #else
         static constexpr const wchar_t *place_holder_width(const char *) { return L"%*s"; }
         static constexpr const wchar_t *place_holder_width(const wchar_t *) { return L"%*ls"; }
    #endif
         static constexpr const wchar_t *place_holder_width(signed short) { return L"%*hd"; }
         static constexpr const wchar_t *place_holder_width(unsigned short) { return L"%*hu"; }
         static constexpr const wchar_t *place_holder_width(signed int) { return L"%*d"; }
         static constexpr const wchar_t *place_holder_width(unsigned int) { return L"%*u"; }
         static constexpr const wchar_t *place_holder_width(signed long) { return L"%*ld"; }
         static constexpr const wchar_t *place_holder_width(unsigned long) { return L"%*lu"; }
         static constexpr const wchar_t *place_holder_width(signed long long) { return L"%*lld"; }
         static constexpr const wchar_t *place_holder_width(unsigned long long) { return L"%*llu"; }
         static constexpr const wchar_t *place_holder_width(double) { return L"%*G"; }
         static constexpr const wchar_t *place_holder_width(long double) { return L"%*lG"; }

         static constexpr const wchar_t *place_holder_precision(void *) { return L"%.*p"; }
    #ifdef _MSC_VER
         static constexpr const wchar_t *place_holder_precision(const char *) { return L"%.*hs"; }
         static constexpr const wchar_t *place_holder_precision(const wchar_t *) { return L"%.*s"; }
    #else
         static constexpr const wchar_t *place_holder_precision(const char *) { return L"%.*s"; }
         static constexpr const wchar_t *place_holder_precision(const wchar_t *) { return L"%.*ls"; }
    #endif
         static constexpr const wchar_t *place_holder_precision(signed short) { return L"%.*hd"; }
         static constexpr const wchar_t *place_holder_precision(unsigned short) { return L"%.*hu"; }
         static constexpr const wchar_t *place_holder_precision(signed int) { return L"%.*d"; }
         static constexpr const wchar_t *place_holder_precision(unsigned int) { return L"%.*u"; }
         static constexpr const wchar_t *place_holder_precision(signed long) { return L"%.*ld"; }
         static constexpr const wchar_t *place_holder_precision(unsigned long) { return L"%.*lu"; }
         static constexpr const wchar_t *place_holder_precision(signed long long) { return L"%.*lld"; }
         static constexpr const wchar_t *place_holder_precision(unsigned long long) { return L"%.*llu"; }
         static constexpr const wchar_t *place_holder_precision(double) { return L"%.*G"; }
         static constexpr const wchar_t *place_holder_precision(long double) { return L"%.*lG"; }

         static constexpr const wchar_t *place_holder_width_precision(void *) { return L"%*.*p"; }
    #ifdef _MSC_VER
         static constexpr const wchar_t *place_holder_width_precision(const char *) { return L"%*.*hs"; }
         static constexpr const wchar_t *place_holder_width_precision(const wchar_t *) { return L"%*.*s"; }
    #else
         static constexpr const wchar_t *place_holder_width_precision(const char *) { return L"%*.*s"; }
         static constexpr const wchar_t *place_holder_width_precision(const wchar_t *) { return L"%*.*ls"; }
    #endif
         static constexpr const wchar_t *place_holder_width_precision(signed short) { return L"%*.*hd"; }
         static constexpr const wchar_t *place_holder_width_precision(unsigned short) { return L"%*.*hu"; }
         static constexpr const wchar_t *place_holder_width_precision(signed int) { return L"%*.*d"; }
         static constexpr const wchar_t *place_holder_width_precision(unsigned int) { return L"%*.*u"; }
         static constexpr const wchar_t *place_holder_width_precision(signed long) { return L"%*.*ld"; }
         static constexpr const wchar_t *place_holder_width_precision(unsigned long) { return L"%*.*lu"; }
         static constexpr const wchar_t *place_holder_width_precision(signed long long) { return L"%*.*lld"; }
         static constexpr const wchar_t *place_holder_width_precision(unsigned long long) { return L"%*.*llu"; }
         static constexpr const wchar_t *place_holder_width_precision(double) { return L"%*.*G"; }
         static constexpr const wchar_t *place_holder_width_precision(long double) { return L"%*.*lG"; }
    };

    /// <summary>
    /// Wraps a generic value for serialization,
    /// packing it along with format information.
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

        // Serializes the held value to text
        template <typename CharType, typename OutType>
        size_t SerializeTo(OutType output) const
        {
            if (m_precision < 0)
            {
                if (m_width < 0)
                    return xprintf(output, PrintFormats<CharType>::place_holder(m_value), m_value);
                else
                    return xprintf(output, PrintFormats<CharType>::place_holder_width(m_value), m_width, m_value);
            }
            else
            {
                if (m_width < 0)
                    return xprintf(output, PrintFormats<CharType>::place_holder_precision(m_value), m_precision, m_value);
                else
                    return xprintf(output, PrintFormats<CharType>::place_holder_width_precision(m_value), m_width, m_precision, m_value);
            }
        }

        // Estimates required size for serialized string
        uint32_t EstimateStringSize() const
        {
            auto width = (m_width >= 0) ? m_width : 0;
            auto precision = (m_precision >= 0) ? m_precision : 0;
            return std::max((uint32_t)std::max(width, precision), (uint32_t)sizeof (ValType));
        }
    };

    // Wraps an argument to prepare for serialization
    template <typename ValType>
    SerializableValue<ValType> FormatArg(ValType value)
    {
        return SerializableValue<ValType>(value);
    }

    // Wraps a string argument to prepare for serialization
    template <typename CharType>
    SerializableValue<const CharType *> FormatArg(const std::basic_string<CharType> &value)
    {
        return SerializableValue<const CharType *>(value.c_str());
    }

    // Just forward an argument which is already wrapped for serialization
    template <typename ValType>
    const SerializableValue<ValType> &FormatArg(const SerializableValue<ValType> &wsval)
    {
        return wsval;
    }

#ifdef _3FD_PLATFORM_WINRT
    // Wraps a string argument to prepare for serialization
    SerializableValue<const wchar_t *> FormatArg(Platform::String ^value);
#endif

    //////////////////////////////
    // Serialization Helpers
    //////////////////////////////

    size_t _estimate_string_size();

    template <typename FirstArgVType, typename ... Args>
    size_t _estimate_string_size(FirstArgVType &&firstArg, Args ... args)
    {
        return FormatArg(firstArg).EstimateStringSize() + _estimate_string_size(args ...);
    }

    template <typename ... Args>
    size_t _estimate_string_size(Args ... args)
    {
        return _estimate_string_size(args ...);
    }

    template <typename CharType>
    constexpr size_t _serialize_to_file_impl(FILE *)
    {
        return 0; // NO-OP
    }

    template <typename CharType, typename FirstArgVType, typename ... Args>
    size_t _serialize_to_file_impl(FILE *file, FirstArgVType &&firstArg, Args ... args)
    {
        auto pcount = FormatArg(firstArg).template SerializeTo<CharType>(file);
        return pcount + _serialize_to_file_impl<CharType>(file, args ...);
    }

    template <typename CharType>
    constexpr size_t _serialize_to_buffer_impl(const RawBufferInfo<CharType> &)
    {
        return 0; // NO-OP
    }

    template <typename CharType, typename FirstArgVType, typename ... Args>
    size_t _serialize_to_buffer_impl(const RawBufferInfo<CharType> &buffer, FirstArgVType &&firstArg, Args ... args)
    {
        auto pcount = FormatArg(firstArg).template SerializeTo<CharType>(buffer);

        if (pcount < buffer.count)
            return pcount + _serialize_to_buffer_impl<CharType>(RawBufferInfo<CharType>{ buffer.data + pcount, buffer.count - pcount }, args ...);
        else
            return pcount + 1; // needs more room
    }

    /// <summary>
    /// Serializes to an output file the argument values as UTF-8 encoded text.
    /// </summary>
    /// <param name="file">The output file.</param>
    /// <param name="...args">The values to serialize, all wrapped in <see cref="SerializableValue" /> objects.</param>
    /// <returns>The length of text written into the output file.</returns>
    template <typename CharType, typename ... Args>
    size_t SerializeTo(FILE *file, Args ... args)
    {
        CALL_STACK_TRACE;
        return _serialize_to_file_impl<CharType>(file, args ...);
    }

    /// <summary>
    /// Serializes to a buffer the argument values as text.
    /// </summary>
    /// <param name="buffer">The output buffer.</param>
    /// <param name="bufCharCount">The buffer size in number of characters.</param>
    /// <param name="...args">The values to serialize, all wrapped in <see cref="SerializableValue" /> objects.</param>
    /// <returns>
    /// The length of text written into the buffer.
    /// </returns>
    template <typename CharType, typename ... Args>
    size_t SerializeTo(CharType buffer[], size_t bufCharCount, Args ... args)
    {
        CALL_STACK_TRACE;

        auto pcount = _serialize_to_buffer_impl(RawBufferInfo<CharType>{ buffer, bufCharCount }, args ...);

        if (pcount < bufCharCount)
            return pcount;
        else
            throw core::AppException<std::logic_error>("Failed to serialize arguments: buffer is too short!");
    }

    /// <summary>
    /// Serializes to a buffer the argument values as text.
    /// </summary>
    /// <param name="buffer">The output buffer.</param>
    /// <param name="...args">The values to serialize, all wrapped in <see cref="SerializableValue" /> objects.</param>
    /// <returns>The length of text written into the buffer.</returns>
    template <typename CharType, size_t N, typename ... Args>
    size_t SerializeTo(std::array<CharType, N> &buffer, Args ... args)
    {
        return SerializeTo(buffer.data(), buffer.size(), args ...);
    }

    /// <summary>
    /// Serializes to a string the argument values as text.
    /// </summary>
    /// <param name="out">The output string.</param>
    /// <param name="...args">The values to serialize, all wrapped in <see cref="SerializableValue" /> objects.</param>
    /// <returns>The length of text written into the string.</returns>
    template <typename CharType, typename ... Args>
    size_t SerializeTo(std::basic_string<CharType> &out, Args ... args)
    {
        CALL_STACK_TRACE;

        try
        {
            /* Try to guarantee room in the string buffer using a rough estimation
            for the serialized string size. If the current size exceeds the estimation,
            do nothing, otherwise, expand it. When resizing, make use of all already
            reserved capacity if that is enough, because such allocation is cheap,
            otherwise, just allocate memory for the estimation and hope for the best.*/

            auto estReqSize = _estimate_string_size(args ...);
            if (out.size() < estReqSize)
            {
                if (out.capacity() < estReqSize)
                    out.resize(estReqSize);
                else
                    out.resize(out.capacity());
            }

            while (true)
            {
                auto pcount = _serialize_to_buffer_impl(RawBufferInfo<CharType>{ &out[0], out.size() }, args ...);

                // string buffer was big enough?
                if (out.size() > pcount)
                {
                    out.resize(pcount);
                    return pcount;
                }
                
                // need more room?
                out.resize(
                    (out.size() * 2 > pcount) ? out.size() * 2 : out.size() + pcount
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
