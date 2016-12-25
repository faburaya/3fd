#include "stdafx.h"
#include "cmdline.h"
#include "callstacktracer.h"
#include <stdexcept>
#include <sstream>
#include <iostream>

namespace _3fd
{
namespace core
{
    /// <summary>
    /// Adds the declaration of an expected command line argument which is
    /// either a switch-type option or an option with required accompanying
    /// value (not limited to a range or set).
    /// </summary>
    /// <param name="argDecl">The argument declaration.</param>
    void CommandLineArguments::AddExpectedArgument(const CommandLineArguments::ArgDeclaration &argDecl)
    {
        CALL_STACK_TRACE;

        const char *stdExMsg("Failed to add declaration of expected command line argument");

        // Argument is switch-type option, but specifies type of accompanying value:
        if (argDecl.type == ArgType::OptionSwitch && argDecl.valueType != ArgValType::None)
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": cannot specify a type for accompanying value when argument is switch-type option";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is option with not required accompanying value, but this function overload does not handle this case:
        if (argDecl.type == ArgType::OptionWithNonReqValue)
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": option with not required accompanying value must specify default value";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is an option, but no valid label (single char or long name) is specified:
        if (static_cast<uint8_t> (argDecl.type) & argIsOptionFlag != 0
            && (argDecl.optName == nullptr || argDecl.optName[0] == 0)
            && !isalnum(argDecl.optChar))
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": an option requires a valid label (single character and/or long name)";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is not an option, but a label for option was specified:
        if (static_cast<uint8_t> (argDecl.type) & argIsOptionFlag == 0
            && ((argDecl.optName != nullptr && argDecl.optName[0] != 0)
                || !isalnum(argDecl.optChar)))
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": a value cannot have a label (neither single character, nor long name)";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is value limited to a range, but this function overload does not handle this case:
        if (static_cast<uint8_t> (argDecl.type) & argIsValueFlag != 0
            && static_cast<uint8_t> (argDecl.valueType) & argValIsRangedTypeFlag != 0)
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": value limited to a range requires specification of boundaries";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is value limited to enumeration, but this function overload does not handle this case:
        if (static_cast<uint8_t> (argDecl.type) & argIsValueFlag != 0
            && static_cast<uint8_t> (argDecl.valueType) & argValIsEnumTypeFlag != 0)
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": value limited to enumeration requires specification of allowed value";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        if (m_expectedArgs.find(argDecl.code) == m_expectedArgs.end())
            m_expectedArgs[argDecl.code] = ArgDeclExtended { argDecl, nullptr };
        else
        {// Collision of argument codes (ID's):
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code << ": collision of ID";
            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }
    }

    // Helps verify consistency of configuration for argument value(s)
    template <typename ValType>
    static void VerifyArgTypedValConfig(
        const CommandLineArguments::ArgDeclaration &argDecl,
        const std::initializer_list<ValType> &argValCfg,
        const char *stdExMsg)
    {
        // Argument is switch-type option, but this function overload does not handle this case:
        if (argDecl.type == CommandLineArguments::ArgType::OptionSwitch)
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": configuration of values does not make sense for switch-type option";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument value does not use default, range or enumeration:
        if (argDecl.type != CommandLineArguments::ArgType::OptionWithNonReqValue
            && static_cast<uint8_t> (argDecl.valueType) & CommandLineArguments::argValIsRangedTypeFlag == 0
            && static_cast<uint8_t> (argDecl.valueType) & CommandLineArguments::argValIsEnumTypeFlag == 0)
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": configuration of values does not make sense unless default value, "
                   "range boundaries or set of allowed values needs to be specified";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        unsigned int minCountValConfigItems(0);

        if (argDecl.type == CommandLineArguments::ArgType::OptionWithNonReqValue)
            ++minCountValConfigItems;

        if (static_cast<uint8_t> (argDecl.valueType) & CommandLineArguments::argValIsEnumTypeFlag != 0)
            ++minCountValConfigItems;
        else if (static_cast<uint8_t> (argDecl.valueType) & CommandLineArguments::argValIsRangedTypeFlag != 0)
            minCountValConfigItems += 2;

        // Argument is supposed to receive configuration of values, but that is incomplete:
        if (argValCfg.size() < minCountValConfigItems)
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": configuration of values has too few items (default, range boundaries, allowed values)";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is an option, but no valid label (single char or long name) is specified:
        if (static_cast<uint8_t> (argDecl.type) & CommandLineArguments::argIsOptionFlag != 0
            && (argDecl.optName == nullptr || argDecl.optName[0] == 0)
            && !isalnum(argDecl.optChar))
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": an option requires a valid label (single character and/or long name)";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is not an option, but a label for option was specified:
        if (static_cast<uint8_t> (argDecl.type) & CommandLineArguments::argIsOptionFlag == 0
            && ((argDecl.optName != nullptr && argDecl.optName[0] != 0)
                || !isalnum(argDecl.optChar)))
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": a value cannot have a label (neither single character, nor long name)";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }
    }

    /// <summary>
    /// Adds the declaration of an expected command line argument which has
    /// an accompanying value of integer type that requires specification of
    /// default value, range or allowed set of values.
    /// </summary>
    /// <param name="argDecl">The argument declaration.</param>
    /// <param name="argValCfg">A list of configured values for the accompanying value.
    /// When such value is not required, the first item is the default value.
    /// When such value is limited to a range, the following 2 items are the min & max.
    /// When such value is limited to a set, the following N (> 1) items are enumerated.</param>
    void CommandLineArguments::AddExpectedArgument(
        const CommandLineArguments::ArgDeclaration &argDecl,
        std::initializer_list<long long> &&argValCfg)
    {
        CALL_STACK_TRACE;

        const char *stdExMsg("Failed to add declaration of expected command line argument");

        // Argument declaration conflicts with value configuration:
        if (static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::Integer) == 0)
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": configuration of integer values conflicts with declaration";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        VerifyArgTypedValConfig(argDecl, argValCfg, stdExMsg);

        AddVerifiedArgSpecIntoMap(argDecl, argValCfg, stdExMsg);
    }

    /// <summary>
    /// Adds the declaration of an expected command line argument which has
    /// an accompanying floating point value that requires specification of
    /// default value, range or allowed set of values.
    /// </summary>
    /// <param name="argDecl">The argument declaration.</param>
    /// <param name="argValCfg">A list of configured values for the accompanying value.
    /// When such value is not required, the first item is the default value.
    /// When such value is limited to a range, the following 2 items are the min & max.
    /// When such value is limited to a set, the following N (> 1) items are enumerated.</param>
    void CommandLineArguments::AddExpectedArgument(const CommandLineArguments::ArgDeclaration &argDecl,
                                                   std::initializer_list<double> &&argValCfg)
    {
        CALL_STACK_TRACE;

        const char *stdExMsg("Failed to add declaration of expected command line argument");

        // Argument declaration conflicts with value configuration:
        if (static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::Float) == 0)
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": configuration of floating point values conflicts with declaration";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        VerifyArgTypedValConfig(argDecl, argValCfg, stdExMsg);

        AddVerifiedArgSpecIntoMap(argDecl, argValCfg, stdExMsg);
    }

    /// <summary>
    /// Adds the declaration of an expected command line argument which has
    /// an accompanying value of string type that requires specification of
    /// default value, range or allowed set of values.
    /// </summary>
    /// <param name="argDecl">The argument declaration.</param>
    /// <param name="argValCfg">A list of configured values for the accompanying value.
    /// When such value is not required, the first item is the default value.
    /// When such value is limited to a range, the following 2 items are the min & max.
    /// When such value is limited to a set, the following N (> 1) items are enumerated.</param>
    void CommandLineArguments::AddExpectedArgument(const CommandLineArguments::ArgDeclaration &argDecl,
                                                   std::initializer_list<const char *> &&argValCfg)
    {
        CALL_STACK_TRACE;

        const char *stdExMsg("Failed to add declaration of expected command line argument");

        // Argument declaration conflicts with value configuration:
        if (static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::String) == 0)
        {
            std::ostringstream oss;
            oss << "Argument code " << argDecl.code
                << ": configuration of string values conflicts with declaration";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        VerifyArgTypedValConfig(argDecl, argValCfg, stdExMsg);

        AddVerifiedArgSpecIntoMap(argDecl, argValCfg, stdExMsg);
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="CommandLineArguments"/> class.
    /// </summary>
    CommandLineArguments::~CommandLineArguments()
    {
        // Release resources from specs of expected arguments:
        for (auto &entry : m_expectedArgs)
        {
            auto &argDeclExt = entry.second;
            
            switch (argDeclExt.common.valueType)
            {
            case ArgValType::Integer:
            case ArgValType::RangeInteger:
            case ArgValType::EnumInteger:
                delete static_cast<std::initializer_list<long long> *> (argDeclExt.typedExtInfo);
                break;
            case ArgValType::Float:
            case ArgValType::RangeFloat:
                delete static_cast<std::initializer_list<double> *> (argDeclExt.typedExtInfo);
                break;
            case ArgValType::String:
            case ArgValType::EnumString:
                delete static_cast<std::initializer_list<const char *> *> (argDeclExt.typedExtInfo);
                break;
            default:
                break;
            }
        }
    }

    /// <summary>
    /// Prints the usage.
    /// </summary>
    void CommandLineArguments::PrintUsage() const
    {
        for (auto &entry : m_expectedArgs)
        {
            auto &argDecl = entry.second.common;

            // Value is integer:
            if (static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::Integer) == 0)
            {
                auto argValCfg = static_cast<std::initializer_list<long long> *> (entry.second.typedExtInfo);

            }

            // Value is floating point:
            if (static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::Float) == 0)
            {
                auto argValCfg = static_cast<std::initializer_list<double> *> (entry.second.typedExtInfo);

            }

            // Value is string:
            if (static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::String) == 0)
            {
                auto argValCfg = static_cast<std::initializer_list<const char *> *> (entry.second.typedExtInfo);
                
            }
            
        }// for loop end
    }

}// end of namespace core
}// end of namespace _3fd
