//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#ifndef UTILS_STRINGS_H // header guard
#define UTILS_STRINGS_H

#include <3fd/utils/serialization.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <cinttypes>
#include <codecvt>
#include <iterator>
#include <map>
#include <string>
#include <string_view>

#ifdef _WIN32
#   define strtok_x strtok_s
#else
#   define strtok_x strtok_r
#endif

namespace _3fd
{
namespace utils
{
    std::string to_lower(std::string str);
    std::string to_upper(std::string str);

    ////////////////////////////////////////////////
    // Type Manipulation
    ////////////////////////////////////////////////

    template <typename Type>
    struct is_string_holder
    {
        typedef Type type;
        typedef const typename std::remove_reference<Type>::type const_non_ref_type;

        static constexpr bool value =
            std::is_same<const_non_ref_type, const std::string>::value
                || std::is_same<const_non_ref_type, const std::string>::value
                || std::is_same<const_non_ref_type, char *const>::value
                || std::is_same<const_non_ref_type, const char *const>::value;
    };

    ////////////////////////////////////////////////
    // Unicode Conversion
    ////////////////////////////////////////////////

    std::wstring to_ucs2(std::string_view input);

    std::string to_utf8(std::wstring_view input);

    // assume that we are using std::string only with UTF-8 content
    inline std::string_view to_utf8(std::string_view input) { return input; }

#ifdef _WIN32
    inline std::wstring to_unicode(std::string_view input) { return to_ucs2(input); }
#else
    inline std::string to_unicode(std::string &&input) { return std::move(input); }
    inline std::string &to_unicode(std::string &input) { return input; }
    inline const std::string &to_unicode(const std::string &input) { return input; }
    inline std::string_view to_unicode(std::string_view input) { return input; }
#endif

    ////////////////////////////////////
    // Placeholder replacement helper
    ////////////////////////////////////

    template <typename string_t, typename string_view_t, typename char_t>
    class TextPlaceholderReplacementHelper
    {
    private:
        // holds the pieces of string to form the final string
        std::vector<string_view_t> m_pieces;

        // maps a placeholder name to the index of a string piece
        std::map<string_view_t, string_t> m_replacements;

        const string_view_t m_referenceText;

        const char_t m_placeholderMarker;

        static bool IsCharAllowedInPlaceholderName(char ch)
        {
            return isalnum(ch) != 0 || ch == '_';
        }

        static bool IsCharAllowedInPlaceholderName(wchar_t ch)
        {
            return iswalnum(ch) != 0 || ch == L'_';
        }

        /// <summary>Constructs a new object by parsing the text and finding the placeholders.</summary>
        /// <param name="placeholderMarker">
        /// The character that marks the start of a placeholder.
        /// Alphanumeric ASCII characters and '_' are the only allowed for a placeholder,
        /// while being forbidden for a marker.
        /// </param>
        /// <param name="text">The text where the placeholders must be replaced.</param>
        TextPlaceholderReplacementHelper(char_t placeholderMarker, string_view_t text)
            : m_referenceText(text)
            , m_placeholderMarker(placeholderMarker)
        {
            size_t offset(0);
            do
            {
                size_t tokenPos = text.find(placeholderMarker, offset);
                if (tokenPos == string_t::npos)
                    tokenPos = text.size();

                // store text piece before token:
                if (tokenPos > offset)
                    m_pieces.push_back(string_view_t(text.data() + offset, tokenPos - offset));

                // parse placeholder:
                ptrdiff_t placeholderLength(0);
                auto placeholderIterBegin = text.begin() + tokenPos;
                if (text.end() != placeholderIterBegin)
                {
                    placeholderLength =
                        std::find_if_not(placeholderIterBegin + 1,
                                            text.end(),
                                            [](char_t ch) { return IsCharAllowedInPlaceholderName(ch); }) -
                        placeholderIterBegin;

                    // store placeholder piece (with marker)
                    m_pieces.push_back(string_view_t(text.data() + tokenPos, placeholderLength));
                }

                offset = tokenPos + placeholderLength;

            } while (offset < text.size());
        }

    public:
        // this template guarantees that only string literals can be used
        template <size_t SizeLiteral>
        static TextPlaceholderReplacementHelper in(char_t placeholderMarker, const char_t (&text)[SizeLiteral])
        {
            return TextPlaceholderReplacementHelper(placeholderMarker, string_view_t(text, SizeLiteral));
        }

        /// <summary>Prepares a replacement of a placeholder by a serialized value.</summary>
        /// <param name="from">The placeholder (without marker!) to replace.</param>
        /// <param name="to">The value to serialize in place.</param>
        /// <returns>A reference to this object (so the calls can be chained.)</returns>
        template <typename TypeReplacement>
        TextPlaceholderReplacementHelper &
        Use(string_view_t from, TypeReplacement toValue)
        {
            std::array<char_t, 32> buffer;
            auto length = SerializeTo(buffer, toValue);
            m_replacements[from] = string_view_t(buffer.data(), length);
            return *this;
        }

        /// <summary>Prepares a replacement of a placeholder by a string.</summary>
        /// <param name="from">The placeholder (without marker!) to replace.</param>
        /// <param name="to">The replacement string.</param>
        /// <returns>A reference to this object (so the calls can be chained.)</returns>
        TextPlaceholderReplacementHelper &Replace(string_view_t from, string_view_t to)
        {
            m_replacements[from] = to;
            return *this;
        }

        /// <summary>Carry out replacements and emit string.</summary>
        /// <returns>String of reference text but with placeholders replaced.</returns>
        string_t Emit() const
        {
            typedef std::basic_ostringstream<char_t,
                std::char_traits<char_t>,
                std::allocator<char_t>> ostringstream;

            ostringstream oss;

            for (auto &piece : m_pieces)
            {
                _ASSERTE(!piece.empty());
                if (piece[0] != m_placeholderMarker)
                {
                    oss << piece;
                }
                else
                {
                    // find placeholder (without marker) among replacements
                    auto iter = m_replacements.find(string_view_t(piece.data() + 1, piece.size() - 1));
                    if (m_replacements.end() != iter)
                        oss << iter->second;
                }
            }

            return oss.str();
        }

    }; // end of class TextPlaceholderReplacementHelper

    typedef TextPlaceholderReplacementHelper<std::string, std::string_view, char> TextUtf8;
    typedef TextPlaceholderReplacementHelper<std::wstring, std::wstring_view, wchar_t> TextUcs2;

    ////////////////////////////////////////////////
    // String Copy Avoidance
    ////////////////////////////////////////////////

    /// <summary>
    /// Holds a string (UTF-8) without taking ownership.
    /// </summary>
    struct CStringViewUtf8
    {
        const char *const data;
        const uint32_t lenBytes;

        CStringViewUtf8(const char *p_data, uint32_t p_lenBytes) noexcept
            : data(p_data), lenBytes(p_lenBytes)
        {
            // cannot have non-zero length when no string is set!
            _ASSERTE(data != nullptr || lenBytes == 0);
        }

        explicit CStringViewUtf8(const char *p_data) noexcept
            : data(p_data), lenBytes(p_data != nullptr ? static_cast<uint32_t>(strlen(data)) : 0)
        {
        }

        explicit CStringViewUtf8(const std::string &str) noexcept
            : data(str.data()), lenBytes(str.length())
        {
        }

        CStringViewUtf8(const char *beginIter, const char *endIter) noexcept
            : data(beginIter), lenBytes(static_cast<uint32_t>(std::distance(beginIter, endIter)))
        {
        }

        constexpr bool null() const noexcept
        {
            return data == nullptr;
        }

        constexpr bool empty() const noexcept
        {
            _ASSERTE(data != nullptr);
            return data[0] == 0;
        }

        constexpr bool null_or_empty() const noexcept
        {
            return null() || empty();
        }

        constexpr const char *begin() const noexcept
        {
            return data;
        }
        constexpr const char *cbegin() const noexcept
        {
            return begin();
        }

        constexpr const char *end() const noexcept
        {
            return data + lenBytes;
        }
        constexpr const char *cend() const noexcept
        {
            return end();
        }

        constexpr char operator[](uint32_t index) const noexcept
        {
            _ASSERTE(data != nullptr && index < lenBytes);
            return data[index];
        }

        std::string to_string() const
        {
            _ASSERTE(data != nullptr);
            return std::string(data, data + lenBytes);
        }
    };

    /// <summary>
    /// Functor "less" for C-style UTF-8 strings.
    /// </summary>
    struct CStringUtf8FunctorLess
    {
        bool operator()(const char *left, const char *right) const
        {
            return strcmp(left, right) < 0;
        }
    };

} // end of namespace utils
} // end of namespace _3fd

#endif // end of header guard
