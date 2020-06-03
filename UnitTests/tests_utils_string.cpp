//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include <3fd/utils/utils_string.h>

namespace _3fd {
namespace unit_tests {

    /// <summary>
    /// Tests <see cref="utils::TextPlaceholderReplacementHelper::Replace"/>.
    /// All placeholders are replaced.
    /// </summary>
    TEST(Framework_Utils_TextPlaceholderReplacementHelper, ReplaceAllPlaceholders_Utf8)
    {
        auto actual =
            utils::TextUtf8::in('$', "Erste: $eins, Zweite: $Zwei, Dritte: $3")
            .Replace("eins", "Platz-1")
            .Replace("Zwei", "Platz-2")
            .Replace("3", "Platz-3")
            .Emit();

        std::string_view expected("Erste: Platz-1, Zweite: Platz-2, Dritte: Platz-3");

        EXPECT_STRCASEEQ(expected.data(), actual.c_str());
    }

    /// <summary>
    /// Tests <see cref="utils::TextPlaceholderReplacementHelper::Replace"/>.
    /// Consecutive placeholders are replaced.
    /// </summary>
    TEST(Framework_Utils_TextPlaceholderReplacementHelper, ReplaceConsecutivePlaceholders_Utf8)
    {
        auto actual =
            utils::TextUtf8::in('$', "Los: $1$2")
            .Replace("1", "Eins")
            .Replace("2", "Zwei")
            .Emit();

        std::string_view expected("Los: EinsZwei");

        EXPECT_STRCASEEQ(expected.data(), actual.c_str());
    }

    /// <summary>
    /// Tests <see cref="utils::TextPlaceholderReplacementHelper::Replace"/>.
    /// Consecutive placeholders are replaced.
    /// </summary>
    TEST(Framework_Utils_TextPlaceholderReplacementHelper, ReplaceConsecutivePlaceholders_Ucs2)
    {
        auto actual =
            utils::TextUcs2::in(L'$', L"Los: $1$2")
            .Replace(L"1", L"Eins")
            .Replace(L"2", L"Zwei")
            .Emit();

        std::wstring_view expected(L"Los: EinsZwei");

        EXPECT_TRUE(wcscmp(actual.c_str(), expected.data()) == 0)
            << '\"' << utils::to_utf8(actual)
            << "\"\nis NOT EQUAL to\n\""
            << utils::to_utf8(expected) << '\"';
    }

    /// <summary>
    /// Tests <see cref="utils::TextPlaceholderReplacementHelper::Replace"/>.
    /// All placeholders are replaced.
    /// </summary>
    TEST(Framework_Utils_TextPlaceholderReplacementHelper, ReplaceAllPlaceholders_Ucs2)
    {
        auto actual =
            utils::TextUcs2::in(L'$', L"Erste: $eins, Zweite: $Zwei, Dritte: $3")
            .Replace(L"eins", L"Platz-1")
            .Replace(L"Zwei", L"Platz-2")
            .Replace(L"3", L"Platz-3")
            .Emit();

        std::wstring_view expected(L"Erste: Platz-1, Zweite: Platz-2, Dritte: Platz-3");
        
        EXPECT_TRUE(wcscmp(actual.c_str(), expected.data()) == 0)
            << '\"' << utils::to_utf8(actual)
            << "\"\nis NOT EQUAL to\n\""
            << utils::to_utf8(expected) << '\"';
    }

    /// <summary>
    /// Tests <see cref="utils::TextPlaceholderReplacementHelper::Replace"/>.
    /// Only some placeholders are replaced.
    /// </summary>
    TEST(Framework_Utils_TextPlaceholderReplacementHelper, ReplaceNotAllPlaceholders_Utf8)
    {
        auto actual =
            utils::TextUtf8::in('$', "Erste: $eins, Zweite: $2, Dritte: $3")
            .Replace("eins", "Platz-1")
            .Emit();

        std::string_view expected("Erste: Platz-1, Zweite: , Dritte: ");

        EXPECT_STRCASEEQ(expected.data(), actual.c_str());
    }

    /// <summary>
    /// Tests <see cref="utils::TextPlaceholderReplacementHelper::Replace"/>.
    /// Only some placeholders are replaced.
    /// </summary>
    TEST(Framework_Utils_TextPlaceholderReplacementHelper, ReplaceNotAllPlaceholders_Ucs2)
    {
        auto actual =
            utils::TextUcs2::in(L'$', L"Erste: $eins, Zweite: $2, Dritte: $3")
            .Replace(L"eins", L"Platz-1")
            .Emit();

        std::wstring_view expected(L"Erste: Platz-1, Zweite: , Dritte: ");

        EXPECT_TRUE(wcscmp(actual.c_str(), expected.data()) == 0)
            << '\"' << utils::to_utf8(actual)
            << "\"\nis NOT EQUAL to\n\""
            << utils::to_utf8(expected) << '\"';
    }

    /// <summary>
    /// Tests <see cref="utils::TextPlaceholderReplacementHelper::use"/>.
    /// </summary>
    TEST(Framework_Utils_TextPlaceholderReplacementHelper, UseNumbers_Utf8)
    {
        auto actual =
            utils::TextUtf8::in('$', "Erste: $1, Zweite: $2, Dritte: $3")
            .Use("1", 1)
            .Use("2", 2.2F)
            .Use("3", -3.3)
            .Emit();

        std::string_view expected("Erste: 1, Zweite: 2.2, Dritte: -3.3");

        EXPECT_STRCASEEQ(expected.data(), actual.c_str());
    }

    /// <summary>
    /// Tests <see cref="utils::TextPlaceholderReplacementHelper::use"/>.
    /// </summary>
    TEST(Framework_Utils_TextPlaceholderReplacementHelper, UseNumbers_Ucs2)
    {
        auto actual =
            utils::TextUcs2::in(L'$', L"Erste: $1, Zweite: $2, Dritte: $3")
            .Use(L"1", 1)
            .Use(L"2", 2.2F)
            .Use(L"3", -3.3)
            .Emit();

        std::wstring_view expected(L"Erste: 1, Zweite: 2.2, Dritte: -3.3");

        EXPECT_TRUE(wcscmp(actual.c_str(), expected.data()) == 0)
            << '\"' << utils::to_utf8(actual)
            << "\"\nis NOT EQUAL to\n\""
            << utils::to_utf8(expected) << '\"';
    }

    /// <summary>
    /// Tests real application of replacing placeholders in SQL.
    /// </summary>
    TEST(Framework_Utils_TextPlaceholderReplacementHelper, SQL)
    {
        std::wstring actual = utils::TextUcs2::in(L'%', LR"(
            if not exists ( select * from sys.service_queues where name = N'%service/v1_0_0/Queue' )
            begin
                create message type [%service/v1_0_0/Message] validation = %validation;
                create contract [%service/v1_0_0/Contract] ([%service/v1_0_0/Message] sent by initiator);
                create queue [%service/v1_0_0/Queue] with poison_message_handling (status = off);
                create service [%service/v1_0_0] on queue [%service/v1_0_0/Queue] ([%service/v1_0_0/Contract]);
            end;

            if not exists (
                select * from sys.systypes
                    where name = N'%service/v1_0_0/Message/ContentType'
            )
            begin
                create type [%service/v1_0_0/Message/ContentType] from varchar(%nbytes);
            end;
        )")
        .Replace(L"service", L"Service")
        .Replace(L"validation", L"StrengeKontrollierung")
        .Use(L"nbytes", 696)
        .Emit();

        std::wstring_view expected(LR"(
            if not exists ( select * from sys.service_queues where name = N'Service/v1_0_0/Queue' )
            begin
                create message type [Service/v1_0_0/Message] validation = StrengeKontrollierung;
                create contract [Service/v1_0_0/Contract] ([Service/v1_0_0/Message] sent by initiator);
                create queue [Service/v1_0_0/Queue] with poison_message_handling (status = off);
                create service [Service/v1_0_0] on queue [Service/v1_0_0/Queue] ([Service/v1_0_0/Contract]);
            end;

            if not exists (
                select * from sys.systypes
                    where name = N'Service/v1_0_0/Message/ContentType'
            )
            begin
                create type [Service/v1_0_0/Message/ContentType] from varchar(696);
            end;
        )");

        EXPECT_TRUE(wcscmp(actual.c_str(), expected.data()) == 0)
            << '\"' << utils::to_utf8(actual)
            << "\"\nis NOT EQUAL to\n\""
            << utils::to_utf8(expected) << '\"';
    }

}// end of namespace unit_tests
}// end of namespace _3fd
