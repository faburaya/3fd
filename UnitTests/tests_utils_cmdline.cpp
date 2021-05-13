//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include <3fd/utils/cmdline.h>
#include <string>
#include <sstream>

namespace _3fd
{
namespace unit_tests
{
    /// <summary>
    /// Helps creating a list of writeable C-Style string out of read-only literals.
    /// </summary>
    class ListOfArguments
    {
    private:
        
        std::vector<std::string> m_strings;

        std::string m_line;

    public:

        ListOfArguments(const std::vector<const char *> &readOnlyArgs)
        {
            std::ostringstream oss;

            m_strings.reserve(readOnlyArgs.size());
            for (auto roarg : readOnlyArgs)
            {
                m_strings.push_back(std::string(roarg));
                oss << roarg << ' ';
            }

            m_line = oss.str();
        }

        std::vector<char *> GetList()
        {
            std::vector<char *> list;
            list.reserve(m_strings.size() + 1);
            for (auto &arg : m_strings)
            {
                list.push_back(arg.data());
            }
            list.push_back(nullptr); // always at the end

            return list;
        }

        const std::string &GetLine() const
        {
            return m_line;
        }

    }; // end of class ListOfArguments

    using _3fd::core::CommandLineArguments;

    enum Sensitivity { CaseSensitive, CaseInsensitive };

    struct CommandLineParserTestParams
    {
        CommandLineArguments::ArgOptionSign optionSign;
        CommandLineArguments::ArgValSeparator valueSeparator;
        std::vector<const char*> args;
        Sensitivity sensitivity;
        bool success = true;
    };

    using Params = CommandLineParserTestParams;

    class TestWithOneParamSwitch
        : public ::testing::TestWithParam<Params>
    {
    };

    /// <summary>
    /// Tests parsing a single parameter which is a switch.
    /// </summary>
    TEST_P(TestWithOneParamSwitch, Parameterized)
    {
        auto params = GetParam();
        CommandLineArguments cmdLineArgs(120,
            params.optionSign,
            params.valueSeparator,
            params.sensitivity == CaseSensitive);

        enum { ArgOptionSwitch };

        cmdLineArgs.AddExpectedArgument(
            CommandLineArguments::ArgDeclaration{
                ArgOptionSwitch,
                CommandLineArguments::ArgType::OptionSwitch,
                CommandLineArguments::ArgValType::None,
                's', "switch",
                "just a switch that can be present or not"
            }
        );


        ListOfArguments args(params.args);
        std::vector<char*> argsList = args.GetList();

        bool status = cmdLineArgs.Parse(argsList.size() - 1, argsList.data());
        EXPECT_EQ(params.success ? STATUS_OKAY : STATUS_FAIL, status) << args.GetLine();

        bool isPresent = cmdLineArgs.GetArgSwitchOptionValue(ArgOptionSwitch);
        EXPECT_EQ(params.success, isPresent);
    }

    INSTANTIATE_TEST_CASE_P(CommandLineParser,
        TestWithOneParamSwitch,
        ::testing::Values(
            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "-s" }, CaseSensitive, true },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "-S" }, CaseInsensitive, true },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "-x" }, CaseInsensitive, false },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "--switch" }, CaseSensitive, true },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "--Switch" }, CaseInsensitive, true },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "--xxx" }, CaseInsensitive, false },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/s" }, CaseSensitive, true },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/S" }, CaseInsensitive, true },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/x" }, CaseInsensitive, false },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/switch" }, CaseSensitive, true },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/Switch" }, CaseInsensitive, true },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/xxx" }, CaseInsensitive, false }
    ));

    class TestWithOneParamNumber
        : public ::testing::TestWithParam<Params>
    {
    };

    /// <summary>
    /// Tests parsing a single parameter which receives a numerical value.
    /// </summary>
    TEST_P(TestWithOneParamNumber, Parameterized)
    {
        auto params = GetParam();
        CommandLineArguments cmdLineArgs(120,
            params.optionSign,
            params.valueSeparator,
            params.sensitivity == CaseSensitive);

        enum { ArgValFloatWithinRange };

        cmdLineArgs.AddExpectedArgument(
            CommandLineArguments::ArgDeclaration{
                ArgValFloatWithinRange,
                CommandLineArguments::ArgType::OptionWithReqValue,
                CommandLineArguments::ArgValType::RangeFloat,
                'n', "number",
                "a floating point value from 0 to 1"
            },
            { 0.5, 0.0, 1.0 }
        );

        struct
        {
            struct
            {
                float number;
            } expected, actual;

            std::vector<char*> args;
        } test;

        ListOfArguments args(params.args);
        test.args = args.GetList();
        test.expected.number = 0.5F;

        bool status = cmdLineArgs.Parse(test.args.size() - 1, test.args.data());

        EXPECT_TRUE(status == STATUS_OKAY) << args.GetLine();
        if (status == STATUS_FAIL)
        {
            std::cerr << "Expected usage:\n";
            cmdLineArgs.PrintArgsInfo();
            FAIL();
        }

        bool isPresent;
        test.actual.number = cmdLineArgs.GetArgValueFloat(ArgValFloatWithinRange, isPresent);
        EXPECT_TRUE(isPresent);
        EXPECT_EQ(test.expected.number, test.actual.number);
    }

    INSTANTIATE_TEST_CASE_P(CommandLineParser,
        TestWithOneParamNumber,
        ::testing::Values(
            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "-n:0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "-n=0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Space,
                    { "program.exe", "-n", "0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "-N:0.5" }, CaseInsensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "--number:0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "--number=0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Space,
                    { "program.exe", "--number", "0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "--Number:0.5" }, CaseInsensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "/n:0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/n=0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Space,
                    { "program.exe", "/n", "0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "/N:0.5" }, CaseInsensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "/number:0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/number=0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Space,
                    { "program.exe", "/number", "0.5" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "/Number:0.5" }, CaseInsensitive }
    ));

    class TestWithOneParamEnumOptions
        : public ::testing::TestWithParam<Params>
    {
    };

    /// <summary>
    /// Tests parsing a single parameter which receives a value out of enumerated options.
    /// </summary>
    TEST_P(TestWithOneParamEnumOptions, Parameterized)
    {
        auto params = GetParam();
        CommandLineArguments cmdLineArgs(120,
            params.optionSign,
            params.valueSeparator,
            params.sensitivity == CaseSensitive);

        enum { ArgValFromEnumStrOptions };

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValFromEnumStrOptions,
            CommandLineArguments::ArgType::OptionWithReqValue,
            CommandLineArguments::ArgValType::EnumString,
            'o', "option",
            "an option from a list of possible strings"
            }, { "option1", "option2" });

        struct
        {
            struct
            {
                const char* chosenOption;
            } expected, actual;

            std::vector<char*> args;
        } test;

        ListOfArguments args(params.args);
        test.args = args.GetList();
        test.expected.chosenOption = "option1";

        bool status = cmdLineArgs.Parse(test.args.size() - 1, test.args.data());

        EXPECT_TRUE(status == STATUS_OKAY) << args.GetLine();
        if (status == STATUS_FAIL)
        {
            std::cerr << "Expected usage:\n";
            cmdLineArgs.PrintArgsInfo();
            FAIL();
        }

        bool isPresent;
        test.actual.chosenOption = cmdLineArgs.GetArgValueString(ArgValFromEnumStrOptions, isPresent);
        EXPECT_TRUE(isPresent);
        EXPECT_STREQ(test.expected.chosenOption, test.actual.chosenOption);
    }

    INSTANTIATE_TEST_CASE_P(CommandLineParser,
        TestWithOneParamEnumOptions,
        ::testing::Values(
            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "-o:option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "-o=option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Space,
                    { "program.exe", "-o", "option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "-O:option1" }, CaseInsensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "--option:option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "--option=option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Space,
                    { "program.exe", "--option", "option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "--Option:option1" }, CaseInsensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "/o:option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/o=option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Space,
                    { "program.exe", "/o", "option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/O=option1" }, CaseInsensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "/option:option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/option=option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Space,
                    { "program.exe", "/option", "option1" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "/Option=option1" }, CaseInsensitive }
    ));

    class TestWithListOfValues
        : public ::testing::TestWithParam<Params>
    {
    };

    /// <summary>
    /// Tests parsing arguments that are a list of values.
    /// </summary>
    TEST_P(TestWithListOfValues, Parameterized)
    {
        auto params = GetParam();
        CommandLineArguments cmdLineArgs(120,
            params.optionSign,
            params.valueSeparator,
            params.sensitivity == CaseSensitive);

        enum { ArgValsListOfStrings };

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValsListOfStrings,
            CommandLineArguments::ArgType::ValuesList,
            CommandLineArguments::ArgValType::String,
            0, "name1 name2",
            "list with two names"
            }, { (uint16_t)2, (uint16_t)2 });

        struct
        {
            struct
            {
                std::vector<const char*> names;
            } expected, actual;

            std::vector<char*> args;
        } test;

        ListOfArguments args(params.args);
        test.args = args.GetList();
        test.expected.names = { "ping", "pong" };

        bool status = cmdLineArgs.Parse(test.args.size() - 1, test.args.data());

        EXPECT_TRUE(status == STATUS_OKAY) << args.GetLine();
        if (status == STATUS_FAIL)
        {
            std::cerr << "Expected usage:\n";
            cmdLineArgs.PrintArgsInfo();
            FAIL();
        }

        cmdLineArgs.GetArgListOfValues(test.actual.names);
        ASSERT_EQ(2, test.actual.names.size());

        for (int idx = 0; idx < test.actual.names.size(); ++idx)
            EXPECT_STREQ(test.expected.names[idx], test.actual.names[idx]);
    }

    INSTANTIATE_TEST_CASE_P(CommandLineParser,
        TestWithListOfValues,
        ::testing::Values(
            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "ping", "pong" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "ping", "pong" }, CaseInsensitive },

            Params{ CommandLineArguments::ArgOptionSign::Dash,
                    CommandLineArguments::ArgValSeparator::Space,
                    { "program.exe", "ping", "pong" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Colon,
                    { "program.exe", "ping", "pong" }, CaseInsensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::EqualSign,
                    { "program.exe", "ping", "pong" }, CaseSensitive },

            Params{ CommandLineArguments::ArgOptionSign::Slash,
                    CommandLineArguments::ArgValSeparator::Space,
                    { "program.exe", "ping", "pong" }, CaseInsensitive }
    ));

    class TestWithSeveralArgumentTypes
        : public ::testing::TestWithParam<Params>
    {
    };

    /// <summary>
    /// Tests parsing a combination of several types of arguments in the command line.
    /// </summary>
    TEST_P(TestWithSeveralArgumentTypes, Parameterized)
    {
        auto params = GetParam();
        CommandLineArguments cmdLineArgs(120,
            params.optionSign,
            params.valueSeparator,
            params.sensitivity == CaseSensitive);

        enum
        {
            ArgOptionSwitch,
            ArgValFromEnumStrOptions,
            ArgValFloatWithinRange,
            ArgValsListOfStrings
        };

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgOptionSwitch,
            CommandLineArguments::ArgType::OptionSwitch,
            CommandLineArguments::ArgValType::None,
            's', "switch",
            "just a switch"
            });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValFromEnumStrOptions,
            CommandLineArguments::ArgType::OptionWithReqValue,
            CommandLineArguments::ArgValType::EnumString,
            'o', "option",
            "an option from a list of possible strings"
        },
        { "option1", "option2" });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValFloatWithinRange,
            CommandLineArguments::ArgType::OptionWithReqValue,
            CommandLineArguments::ArgValType::RangeFloat,
            'n', "number",
            "a floating point value from 0 to 1"
        },
        { 0.5, 0.0, 1.0 });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValsListOfStrings,
            CommandLineArguments::ArgType::ValuesList,
            CommandLineArguments::ArgValType::String,
            0, "name1 name2",
            "list with two names"
        }, { (uint16_t)2, (uint16_t)2 });

        struct
        {
            struct
            {
                const char *chosenOption;
                float number;
                std::vector<const char *> names;
            } expected, actual;

            std::vector<char *> args;
        } test;

        ListOfArguments args(params.args);
        test.args = args.GetList();

        test.expected.chosenOption = "option1";
        test.expected.number = 0.5F;
        test.expected.names = { "ping", "pong" };

        bool status = cmdLineArgs.Parse(test.args.size() - 1, test.args.data());

        EXPECT_TRUE(status == STATUS_OKAY) << args.GetLine();
        if (status == STATUS_FAIL)
        {
            std::cerr << "Expected usage:\n";
            cmdLineArgs.PrintArgsInfo();
            FAIL();
        }

        bool isPresent = cmdLineArgs.GetArgSwitchOptionValue(ArgOptionSwitch);
        EXPECT_TRUE(isPresent);

        test.actual.chosenOption = cmdLineArgs.GetArgValueString(ArgValFromEnumStrOptions, isPresent);
        EXPECT_TRUE(isPresent);
        EXPECT_STREQ(test.expected.chosenOption, test.actual.chosenOption);

        test.actual.number = cmdLineArgs.GetArgValueFloat(ArgValFloatWithinRange, isPresent);
        EXPECT_TRUE(isPresent);
        EXPECT_EQ(test.expected.number, test.actual.number);

        cmdLineArgs.GetArgListOfValues(test.actual.names);
        ASSERT_EQ(2, test.actual.names.size());

        for (int idx = 0; idx < test.actual.names.size(); ++idx)
            EXPECT_STREQ(test.expected.names[idx], test.actual.names[idx]);
    }

    INSTANTIATE_TEST_CASE_P(CommandLineParser, TestWithSeveralArgumentTypes, ::testing::Values(
        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::Colon,
                { "program.exe", "-s", "-o:option1", "-n:0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::Colon,
                { "program.exe", "-S", "-O:option1", "-N:0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::EqualSign,
                { "program.exe", "-s", "-o=option1", "-n=0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::EqualSign,
                { "program.exe", "-S", "-O=option1", "-N=0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::Space,
                { "program.exe", "-s", "-o", "option1", "-n", "0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::Space,
                { "program.exe", "-S", "-O", "option1", "-N", "0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::Colon,
                { "program.exe", "--switch", "--option:option1", "--number:0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::Colon,
                { "program.exe", "--Switch", "--Option:option1", "--Number:0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::EqualSign,
                { "program.exe", "--switch", "--option=option1", "--number=0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::EqualSign,
                { "program.exe", "--Switch", "--Option=option1", "--Number=0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::Space,
                { "program.exe", "--switch", "--option", "option1", "--number", "0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Dash,
                CommandLineArguments::ArgValSeparator::Space,
                { "program.exe", "--Switch", "--Option", "option1", "--Number", "0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::Colon,
                { "program.exe", "/s", "/o:option1", "/n:0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::Colon,
                { "program.exe", "/S", "/O:option1", "/N:0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::EqualSign,
                { "program.exe", "/s", "/o=option1", "/n=0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::EqualSign,
                { "program.exe", "/S", "/O=option1", "/N=0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::Space,
                { "program.exe", "/s", "/o", "option1", "/n", "0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::Space,
                { "program.exe", "/S", "/O", "option1", "/N", "0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::Colon,
                { "program.exe", "/switch", "/option:option1", "/number:0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::Colon,
                { "program.exe", "/Switch", "/Option:option1", "/Number:0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::EqualSign,
                { "program.exe", "/switch", "/option=option1", "/number=0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::EqualSign,
                { "program.exe", "/Switch", "/Option=option1", "/Number=0.5", "ping", "pong" }, CaseInsensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::Space,
                { "program.exe", "/switch", "/option", "option1", "/number", "0.5", "ping", "pong" }, CaseSensitive },

        Params{ CommandLineArguments::ArgOptionSign::Slash,
                CommandLineArguments::ArgValSeparator::Space,
                { "program.exe", "/Switch", "/Option", "option1", "/Number", "0.5", "ping", "pong" }, CaseInsensitive }));

} // end of namespace unit_tests
} // end of namespace _3fd