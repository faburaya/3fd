#include "stdafx.h"
#include "cmdline.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <vector>
#include <regex>

namespace _3fd
{
namespace core
{
    static bool isNullOrEmpty(const char *str)
    {
        return str == nullptr || str[0] == 0;
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="CommandLineArguments" /> class.
    /// </summary>
    /// <param name="appName">Name of the application executable.</param>
    /// <param name="minCmdLineWidth">Minimum width of the command line.</param>
    /// <param name="argValSeparator">The character separator to use between option label and value.</param>
    /// <param name="useOptSignSlash">if set to <c>true</c>, use option sign slash (Windows prompt style) instead of dash.</param>
    /// <param name="optCaseSensitive">if set to <c>true</c>, make case sensitive the parsing of single character labels for options.</param>
    CommandLineArguments::CommandLineArguments(uint8_t minCmdLineWidth,
                                               ArgValSeparator argValSeparator,
                                               bool useOptSignSlash,
                                               bool optCaseSensitive) :
        m_minCmdLineWidth(minCmdLineWidth > 80 ? minCmdLineWidth : 80),
        m_argValSeparator(argValSeparator),
        m_useOptSignSlash(useOptSignSlash),
        m_isOptCaseSensitive(optCaseSensitive),
        m_largestNameLabel(0),
        m_idValueTypeArg(-1)
    {
    }

    // Looks for inconsistencies that are common to declarations of all types of arguments
    static void LookForCommonInconsistencies(const CommandLineArguments::ArgDeclaration &argDecl,
                                             const char *stdExMsg)
    {
        CALL_STACK_TRACE;

        // Argument has empty description:
        if (isNullOrEmpty(argDecl.description))
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id << ": description cannot be empty";
            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is an option, but no valid label (single char or long name) is specified:
        if ((static_cast<uint8_t> (argDecl.type) & CommandLineArguments::argIsOptionFlag) != 0
            && isNullOrEmpty(argDecl.optName)
            && !isalnum(argDecl.optChar))
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": an option requires a valid label (single character and/or name)";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        if ((static_cast<uint8_t> (argDecl.type) & CommandLineArguments::argIsOptionFlag) == 0)
        {
            // Single char label was specified, bur argument is a value:
            if (argDecl.optChar != 0)
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id << ": value cannot have a single character label";
                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }

            // Argument is a value, but a name label was not specified:
            if (isNullOrEmpty(argDecl.optName))
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id << ": value must have a name label";
                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }

            // Argument is a value, but also limited to a range or enumeration:
            if ((static_cast<uint8_t> (argDecl.valueType) & CommandLineArguments::argValIsRangedTypeFlag) != 0
                || (static_cast<uint8_t> (argDecl.valueType) & CommandLineArguments::argValIsEnumTypeFlag) != 0)
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id << ": a value argument cannot be limited to a range or enumeration";
                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }
        }
    }

    /// <summary>
    /// Validates the argument labels against previously added ones, issuing
    /// an error whenever collision occurs. The labels themselves are also
    /// verified for forbidden characters or whether too big, as well as
    /// the description.
    /// </summary>
    /// <param name="argDecl">The argument declaration.</param>
    /// <param name="stdExMsg">The standard message for thrown exceptions.</param>
    void CommandLineArguments::ValidateArgDescAndLabels(const CommandLineArguments::ArgDeclaration &argDecl,
                                                        const char *stdExMsg)
    {
        CALL_STACK_TRACE;

        bool nonWhiteSpaceFound(false);
        char ch;
        uint32_t charCount(0);

        // Compute length of description while checking content:
        while ((ch = *(argDecl.description + charCount)) != 0)
        {
            nonWhiteSpaceFound = (iswspace(ch) == 0);
            ++charCount;
        }

        // description has only white spaces?
        if (!nonWhiteSpaceFound)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id << ": description cannot be empty";
            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        const uint32_t maxLengthArgDesc(2000);

        // is description too large?
        if (charCount > maxLengthArgDesc)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": description is too large (limit is "
                << maxLengthArgDesc << " UTF-8 characters)";

            throw AppException<std::length_error>(stdExMsg, oss.str());
        }

        // single char label specified?
        if (argDecl.optChar != 0)
        {
            auto iter = m_argsByCharLabel.find(argDecl.optChar);

            // collides with another single char label?
            if (m_argsByCharLabel.end() == iter)
                m_argsByCharLabel[argDecl.optChar] = argDecl.id;
            else
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id
                    << ": single character label '" << argDecl.optChar
                    << "' is already in use by argument ID " << iter->second;

                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }
        }

        // name label not specified?
        if (isNullOrEmpty(argDecl.optName))
            return;

        auto iter = m_argsByNameLabel.find(argDecl.optName);

        // colides with another name label?
        if (m_argsByNameLabel.end() == iter)
            m_argsByNameLabel[argDecl.optName] = argDecl.id;
        else
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": name label '" << argDecl.optName
                << "' is already in use by argument ID " << iter->second;

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        /* Windows notation in command line normally uses a slash to mark options,
           and adopts an underline when composing names like 'no_warnings', which
           otherwise would require undesired camel notation for better readability.
           POSIX notation, in the other hand, chooses a dash for the same purpose,
           like in '--no-warnings'. Those practices are enforced as rules here, so
           the application is made compliant with these standards. */
        const char dash = (m_useOptSignSlash) ? '_' : '-';
        
        // Compute length of name label while checking for disallowed chars:
        charCount = 0;
        while ((ch = *(argDecl.optName + charCount)) != 0)
        {
            // alphanumeric ASCII chars are allowed
            if (isalnum(ch) || ch == dash)
                ++charCount;
            // spaces are allowed when the argument is a list of values:
            else if (iswspace(ch))
            {
                if (argDecl.type == ArgType::ValuesList)
                    ++charCount;
                else
                {
                    std::ostringstream oss;
                    oss << "Argument ID " << argDecl.id
                        << ": white spaces in name label are allowed only for arguments that are a list of values";

                    throw AppException<std::invalid_argument>(stdExMsg, oss.str());
                }
            }
            else // disallowed character:
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id
                    << ": only alphanumeric ASCII characters (and dash for POSIX "
                       "option notation which also uses dash, or underline for Windows "
                       "option notation which uses slash)  are allowed in name label";

                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }
        }

        const uint32_t maxLengthNameLabel(24);

        // is label too big?
        if (charCount > maxLengthNameLabel)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": name label is too large (limit is "
                << maxLengthNameLabel << " UTF-8 characters)";

            throw AppException<std::length_error>(stdExMsg, oss.str());
        }

        /* Keep the largest name label. Later this will be used to plan
           the layout of a pretty table of arguments when printing the
           usage of command line. */
        if (charCount > m_largestNameLabel)
            m_largestNameLabel = charCount;
    }

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
            oss << "Argument ID " << argDecl.id
                << ": cannot specify a type for accompanying value when argument is switch-type option";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is a list of values, but this function overload does not handle this case:
        if (argDecl.type == ArgType::ValuesList)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": list of values requires specification of min & max count of items";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is value limited to a range, but this function overload does not handle this case:
        if ((static_cast<uint8_t> (argDecl.valueType) & argValIsRangedTypeFlag) != 0)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": value limited to a range requires specification of boundaries";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument is value limited to enumeration, but this function overload does not handle this case:
        if ((static_cast<uint8_t> (argDecl.valueType) & argValIsEnumTypeFlag) != 0)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": value limited to enumeration requires specification of allowed value";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        LookForCommonInconsistencies(argDecl, stdExMsg);

        ValidateArgDescAndLabels(argDecl, stdExMsg);

        if (m_expectedArgs.find(argDecl.id) == m_expectedArgs.end())
            m_expectedArgs[argDecl.id] = ArgDeclExtended { argDecl, nullptr };
        else
        {// Collision of argument codes (ID's):
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id << ": collision of ID";
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
            oss << "Argument ID " << argDecl.id
                << ": configuration of values does not make sense for switch-type option";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        // Argument value is limited to an enumeration
        if ((static_cast<uint8_t> (argDecl.valueType) & CommandLineArguments::argValIsEnumTypeFlag) != 0)
        {
            // Configuration of values needs at least 1 value, which is also default:
            if (argValCfg.size() < 1)
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id << ": configuration of values must specify at least one allowed value";
                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }
        }
        // Argument values are limited to a range
        else if ((static_cast<uint8_t> (argDecl.valueType) & CommandLineArguments::argValIsRangedTypeFlag) != 0)
        {
            // Configuration of range of values is wrong:
            if (argValCfg.size() < 2 || argValCfg.size() > 3)
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id << ": configuration of values must be {[default,] min, max}";
                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }

            auto min = *(argValCfg.begin() + (argValCfg.size() == 3 ? 1 : 0));
            auto max = *(min + 1);

            // range boundaries in wrong order?
            if (min >= max)
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id << ": defined range boundaries are incoherent (descendent order)";
                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }

            // Default value does not fall into defined range:
            if (argValCfg.size() == 3)
            {
                auto default = *(argValCfg.begin());

                if (default < min || default > max)
                {
                    std::ostringstream oss;
                    oss << "Argument ID " << argDecl.id << ": default value does not fall into defined range";
                    throw AppException<std::invalid_argument>(stdExMsg, oss.str());
                }
            }
        }
        // Argument is a list of values
        else if (argDecl.type == CommandLineArguments::ArgType::ValuesList)
        {
            // Configuration of items count is wrong:
            if (argValCfg.size() != 2)
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id << ": configuration of values must be {min count, max count}";
                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }

            auto min = *(argValCfg.begin());
            auto max = *(min + 1);

            // min and max in wrong order?
            if (min == 0 || min > max)
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id
                    << ": defined min and max count of items is incoherent "
                       "(the min must be at least 1 and come before max)";

                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }
        }
        else
        {/* config provided but not a list of values, neither limited to range
            nor enumeration? then it has to be a single default value: */
            if (argValCfg.size() > 1)
            {
                std::ostringstream oss;
                oss << "Argument ID " << argDecl.id << ": configuration of values has too many items";
                throw AppException<std::invalid_argument>(stdExMsg, oss.str());
            }
        }
    }

    /// <summary>
    /// Adds the declaration of an expected command line argument which has
    /// an accompanying value of integer type that requires specification of
    /// range or allowed set of values.
    /// </summary>
    /// <param name="argDecl">The argument declaration.</param>
    /// <param name="argValCfg">A list of configured values for the accompanying value.
    /// When such value is limited to a range, the items are the min & max. (if triplet, first is default)
    /// When such value is limited to a set, the items are the set of allowed values. (first is default)</param>
    void CommandLineArguments::AddExpectedArgument(const ArgDeclaration &argDecl,
                                                   std::initializer_list<long long> &&argValCfg)
    {
        CALL_STACK_TRACE;

        const char *stdExMsg("Failed to add declaration of expected command line argument");

        // Argument declaration conflicts with value configuration:

        if ((static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::Integer)) == 0)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": configuration of integer values conflicts with declaration";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        if (argDecl.type == CommandLineArguments::ArgType::ValuesList)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": list of values requires configured values to be min and max "
                "count of items, expressed as unsigned integers 16 bits long";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        LookForCommonInconsistencies(argDecl, stdExMsg);

        VerifyArgTypedValConfig(argDecl, argValCfg, stdExMsg);

        AddVerifiedArgSpecIntoMap(argDecl, argValCfg, stdExMsg);
    }

    /// <summary>
    /// Adds the declaration of an expected command line argument which has
    /// an accompanying floating point value that requires specification of
    /// range or allowed set of values.
    /// </summary>
    /// <param name="argDecl">The argument declaration.</param>
    /// <param name="argValCfg">A list of configured values for the accompanying value.
    /// When such value is limited to a range, the items are the min & max. (if triplet, first is default)
    /// When such value is limited to a set, the items are the set of allowed values. (first is default)</param>
    void CommandLineArguments::AddExpectedArgument(const ArgDeclaration &argDecl,
                                                   std::initializer_list<double> &&argValCfg)
    {
        CALL_STACK_TRACE;

        const char *stdExMsg("Failed to add declaration of expected command line argument");

        // Argument declaration conflicts with value configuration:

        if ((static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::Float)) == 0)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": configuration of floating point values conflicts with declaration";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        if (argDecl.type == CommandLineArguments::ArgType::ValuesList)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": list of values requires configured values to be min and max "
                "count of items, expressed as unsigned integers 16 bits long";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        LookForCommonInconsistencies(argDecl, stdExMsg);

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
    void CommandLineArguments::AddExpectedArgument(const ArgDeclaration &argDecl,
                                                   std::initializer_list<const char *> &&argValCfg)
    {
        CALL_STACK_TRACE;

        const char *stdExMsg("Failed to add declaration of expected command line argument");

        // Argument declaration conflicts with value configuration:

        if ((static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::String)) == 0)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": configuration of string values conflicts with declaration";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }
        
        if (argDecl.type == CommandLineArguments::ArgType::ValuesList)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": list of values requires configured values to be min and max "
                "count of items, expressed as unsigned integers 16 bits long";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        LookForCommonInconsistencies(argDecl, stdExMsg);

        VerifyArgTypedValConfig(argDecl, argValCfg, stdExMsg);

        AddVerifiedArgSpecIntoMap(argDecl, argValCfg, stdExMsg);
    }

    /// <summary>
    /// Adds the declaration of an expected command line argument which
    /// is a list of values of any type, that requires specification of
    /// minimum and maximum count of items.
    /// </summary>
    /// <param name="argDecl">The argument declaration.</param>
    /// <param name="argValCfg">A list of configured values for
    /// the argument, which specifies (in this order) the minimum
    /// and maximum count of items it must support.</param>
    void CommandLineArguments::AddExpectedArgument(const ArgDeclaration &argDecl,
                                                   std::initializer_list<uint16_t> &&argValCfg)
    {
        CALL_STACK_TRACE;

        const char *stdExMsg("Failed to add declaration of expected command line argument");

        // Argument declaration conflicts with value configuration:
        if (argDecl.type != CommandLineArguments::ArgType::ValuesList)
        {
            std::ostringstream oss;
            oss << "Argument ID " << argDecl.id
                << ": configuration of short integer values conflicts with declaration";

            throw AppException<std::invalid_argument>(stdExMsg, oss.str());
        }

        LookForCommonInconsistencies(argDecl, stdExMsg);

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

            if (argDeclExt.common.type == ArgType::ValuesList)
            {
                delete static_cast<std::initializer_list<uint16_t> *> (argDeclExt.typedExtInfo);
                continue;
            }
            
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

    /* Prints configuration of argument values. Notice that at this point all
       the validation has already taken place, so it is possible to handle info
       without so many checks for null pointers and boundaries. */
    template <typename ValType>
    static void PrintArgValuesConfig(const CommandLineArguments::ArgDeclaration &argDecl,
                                     const std::initializer_list<ValType> &argValCfg,
                                     std::ostringstream &oss,
                                     const char *stdExMsg)
    {
        CALL_STACK_TRACE;

        auto iter = argValCfg.begin();

        // Value is limited to a range
        if ((static_cast<uint8_t> (argDecl.valueType) & CommandLineArguments::argValIsRangedTypeFlag) != 0)
        {
            oss << " (";

            if (argValCfg.size() == 3)
                oss << "when ommited, default = " << *iter++ << "; ";

            auto rangeMinIter = iter;
            auto rangeMaxIter = iter + 1;
            oss << "min = " << *rangeMinIter << "; max = " << *rangeMaxIter << ')';
        }
        // Value is limited to enumeration
        else if ((static_cast<uint8_t> (argDecl.valueType) & CommandLineArguments::argValIsEnumTypeFlag) != 0)
        {
            oss << "(allowed: [(default = )";
            iter = argValCfg.begin();
            do
            {
                oss << *iter << (argValCfg.end() != iter + 1 ? ", " : "])");
            }
            while (argValCfg.end() != ++iter);
        }
        // List of values
        else if (argDecl.type == CommandLineArguments::ArgType::ValuesList)
        {
            auto minCountIter = iter;
            auto maxCountIter = iter + 1;

            if (*minCountIter != *maxCountIter)
                oss << "(from  " << *minCountIter << " to " << *maxCountIter << " values";
            else
                oss << "(expects " << *minCountIter << " values)";
        }
    }

    /// <summary>
    /// Formats the text contained in the provided string stream, placing it as
    /// a paragraph with a maximum given width (all lines) and advanced to the
    /// right by a given amount of spaces (from the second line onwards).
    /// </summary>
    /// <param name="oss">The string stream, which is supposed not to contain line breaks.</param>
    /// <param name="width">The paragraph width.</param>
    /// <param name="advance">The amount of spaces to advance to the right.</param>
    /// <param name="stdExMsg">The standard message for thrown exceptions.</param>
    /// <returns>A string containing the formatted paragraph.</returns>
    static string FormatParagraph(std::ostringstream &oss,
                                  uint16_t width,
                                  uint16_t advance,
                                  const char *stdExMsg)
    {
        CALL_STACK_TRACE;

        auto input = oss.str();
        oss.str("");

        // if all content fits into one line, do nothing:
        auto length = input.length();
        if (length <= width)
            return input;
        
        /* Assuming that width is not too narrow and the words in the text are not too
           large, then this estimate for output buffer size should give some extra room
           to safely prevent writing memory out-of-bound. */
        auto outBufSize = static_cast<size_t> (2 * (1 + length / width) * (advance + width));
        
        /* Allocate memory already zeroed, so finishing the
           string with a null char will not be necessary */
        char *output = (char *)calloc(outBufSize, sizeof(char));

        if (output == nullptr)
        {
            std::ostringstream oss;
            oss << stdExMsg << ": failed to allocate memory";
            throw AppException<std::runtime_error>(oss.str());
        }

        try
        {
            auto outIter = output;
            auto inIter = input.data();

            // Break text into lines not bigger than paragraph width:
            do
            {
                bool whiteSpaceFound(false);

                /* search the line backwards to find a white space
                to break the line, but do not go past the middle: */
                uint16_t idx(width - 1);
                while (idx > width / 2)
                {
                    if (iswspace(*(inIter + idx)) != 0)
                        whiteSpaceFound = true;

                    if (!whiteSpaceFound)
                        --idx;
                    else
                        break;
                }

                /* if a white space could not be found,
                the line will break inside a word: */
                if (!whiteSpaceFound)
                    idx = width;

                // check for room before writing to output:
                if (outIter + idx >= output + outBufSize)
                {
                    std::ostringstream oss;
                    oss << stdExMsg << ": buffer is too short!";
                    throw AppException<std::logic_error>(oss.str());
                }

                strncpy(outIter, inIter, idx); // copy slice from input to output
                outIter += idx; // move output iterator past the slice
                *outIter = '\n'; // break the line
                ++outIter; // move output iterator past line break

                inIter += idx; // move input iterator past the slice
                length -= idx; // update remaining length

                // skip white space:
                if (whiteSpaceFound)
                {
                    ++inIter;
                    --length;
                }

                // check for room before writing to output:
                if (outIter + advance > output + outBufSize)
                {
                    std::ostringstream oss;
                    oss << stdExMsg << ": buffer is too short!";
                    throw AppException<std::logic_error>(oss.str());
                }

                // if any content remains, prepare the next line:
                if (length > 0)
                {
                    memset(outIter, ' ', advance); // pad with spaces to advance next line to the right
                    outIter += advance;
                }

            } while (length > width); // continue breaking lines?

            // copy the last line to output:
            if (length > 0)
            {
                // check for room before writing to output:
                if (outIter + length > output + outBufSize)
                {
                    std::ostringstream oss;
                    oss << stdExMsg << ": buffer is too short!";
                    throw AppException<std::logic_error>(oss.str());
                }

                strncpy(outIter, inIter, length);
            }

            string formattedParagraph(output);
            free(output);

            return formattedParagraph;
        }
        catch (IAppException &)
        {
            free(output);
            throw;
        }
    }

    /// <summary>
    /// Prints information about command line arguments.
    /// </summary>
    void CommandLineArguments::PrintArgsInfo() const
    {
        CALL_STACK_TRACE;

        const char *stdExMsg("Could not print usage of command line arguments");

        const char optCharSign(m_useOptSignSlash ? '/' : '-'); // sign for option with single char label

        const char *spaceAdvLeftBorder(" "), // space advanced by left border of table column 1
                   *commaBetweenLabels(", "), // comma placed between single char and name labels
                   *optNameSign(m_useOptSignSlash ? "/" : "--"), // sign for option with name label
                   *spaceBetweenCols("   "); // space between tables columns 1 & 2

        uint16_t widthTableCol1;

        // any name label is present?
        if (m_largestNameLabel > 0)
        {
            widthTableCol1 = static_cast<uint16_t> (
                strlen(spaceAdvLeftBorder) + 2
                + strlen(commaBetweenLabels)
                + strlen(optNameSign)
                + m_largestNameLabel
                + (m_argValSeparator == ArgValSeparator::Space ? 0 : 2)
            );
        }
        else
        {
            widthTableCol1 = static_cast<uint16_t> (
                strlen(spaceAdvLeftBorder) + 2
                + (m_argValSeparator == ArgValSeparator::Space ? 0 : 1)
            );
        }

        // amount of spaces a paragraph in col 2 is advanced to the right
        auto nSpacesAdvCol2 = static_cast<uint16_t> (widthTableCol1 + strlen(spaceBetweenCols));

        // width of table col 2, where the paragraphs must be formatted
        auto widthTableCol2 = static_cast<uint16_t> (m_minCmdLineWidth - nSpacesAdvCol2);

        try
        {
            std::ostringstream oss;

            for (auto &entry : m_expectedArgs)
            {
                ////////////////////////
                // Print first column:

                auto &argDecl = entry.second.common;

                // single char label present?
                if (argDecl.optChar != 0)
                {
                    oss << optCharSign << argDecl.optChar;

                    // value separator needed?
                    if (argDecl.valueType != ArgValType::None && m_argValSeparator != ArgValSeparator::Space)
                        oss << static_cast<char> (m_argValSeparator);

                    // name label present?
                    if (!isNullOrEmpty(argDecl.optName))
                        oss << commaBetweenLabels;
                }

                // name label present?
                if (!isNullOrEmpty(argDecl.optName))
                {
                    // option?
                    if ((static_cast<uint8_t> (argDecl.type) & argIsOptionFlag) != 0)
                        oss << optNameSign;

                    oss << argDecl.optName;

                    // value separator needed?
                    if ((static_cast<uint8_t> (argDecl.type) & argIsOptionFlag) != 0
                        && argDecl.valueType != ArgValType::None
                        && m_argValSeparator != ArgValSeparator::Space)
                    {
                        oss << static_cast<char> (m_argValSeparator);
                    }
                }

                std::cout << std::setw(widthTableCol1) << oss.str() << spaceBetweenCols;

                ////////////////////////
                // Print second column:

                oss.str("");
                oss << std::skipws << argDecl.description;

                // configuration for values available?
                if (entry.second.typedExtInfo != nullptr)
                {
                    // list of values?
                    if (argDecl.type == ArgType::ValuesList)
                    {
                        auto &argValCfg = *static_cast<std::initializer_list<uint16_t> *> (entry.second.typedExtInfo);
                        PrintArgValuesConfig(argDecl, argValCfg, oss, stdExMsg);
                    }
                    // is value an integer?
                    if ((static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::Integer)) != 0)
                    {
                        auto &argValCfg = *static_cast<std::initializer_list<long long> *> (entry.second.typedExtInfo);
                        PrintArgValuesConfig(argDecl, argValCfg, oss, stdExMsg);
                    }
                    // is value a floating point?
                    else if ((static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::Float)) != 0)
                    {
                        auto &argValCfg = *static_cast<std::initializer_list<double> *> (entry.second.typedExtInfo);
                        PrintArgValuesConfig(argDecl, argValCfg, oss, stdExMsg);
                    }
                    // is value a string?
                    else if ((static_cast<uint8_t> (argDecl.valueType) & static_cast<uint8_t> (ArgValType::String)) != 0)
                    {
                        auto &argValCfg = *static_cast<std::initializer_list<const char *> *> (entry.second.typedExtInfo);
                        PrintArgValuesConfig(argDecl, argValCfg, oss, stdExMsg);
                    }
                }

                std::cout << FormatParagraph(oss, widthTableCol2, nSpacesAdvCol2, stdExMsg) << '\n' << std::endl;

            }// for loop end
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions for errors known to have been already handled
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << stdExMsg << ": " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

    // Parses the string of a regex submatch into an integer value
    static bool ParseInteger(const std::csub_match &matchVal, long long &value)
    {
        char *strEnd;
        value = strtoll(matchVal.first, &strEnd, 0);

        if (strEnd == matchVal.second)
            return STATUS_OKAY;
        else
        {
            std::cerr << "Parser error: '" << matchVal.first << "' is not a valid integer value";
            return STATUS_FAIL;
        }
    }

    // Parses the string of a regex submatch into a floating point value
    static bool ParseFloat(const std::csub_match &matchVal, double &value)
    {
        char *strEnd;
        value = strtod(matchVal.first, &strEnd);

        if (strEnd == matchVal.second)
            return STATUS_OKAY;
        else
        {
            std::cerr << "Parser error: '" << matchVal.first << "' is not a valid floating point value";
            return STATUS_FAIL;
        }
    }

    // Helps validating whether an argument value belongs to its set of allowed value
    template <typename ValType>
    static void ValidateEnumValue(const CommandLineArguments::ArgDeclaration &argDecl,
                                  const std::initializer_list<ValType> &argValCfg,
                                  ValType value)
    {
        auto iter = std::find(argValCfg.begin(), argValCfg.end(), value);

        if (iter == end)
        {
            std::cerr << "Parser error: '" << value
                      << "' does not belong to the allowed set of values for command line option '";

            if (!isNullOrEmpty(argDecl.optName))
                std::cerr << argDecl.optName;
            else
                std::cerr << argDecl.optChar;

            std::cerr << '\'' << std::endl;
            return STATUS_FAIL;
        }

        return STATUS_OKAY;
    }

    // Helps validating whether an argument value falls into the configured range
    template <typename ValType>
    static void ValidateRangedValue(const CommandLineArguments::ArgDeclaration &argDecl,
                                    const std::initializer_list<ValType> &argValCfg,
                                    ValType value)
    {
        auto iterMin = argValCfg.begin() + (argValCfg.size() - 2);
        auto iterMax = iterMin + 1;

        if (value < *iterMin || value > *iterMax)
        {
            std::cerr << "Parser error: '" << value
                      << "' does not fall into range configured for command line option '";

            if (!isNullOrEmpty(argDecl.optName))
                std::cerr << argDecl.optName;
            else
                std::cerr << argDecl.optChar;

            std::cerr << '\'' << std::endl;
            return STATUS_FAIL;
        }

        return STATUS_OKAY;
    }

    // Helps parsing a value of given type, then validates it against configuration
    static bool ParseAndValidateValue(const CommandLineArguments::ArgDeclaration &argDecl,
                                      void *argValCfg,
                                      const std::csub_match &matchVal,
                                      CommandLineArguments::ParsedValue &parsedValue)
    {
        // this implementation should only be called to parse values of arguments that expect and matched one
        _ASSERTE(argDecl.valueType != CommandLineArguments::ArgValType::None && matchVal.matched);

        CALL_STACK_TRACE;

        char *strEnd;

        switch (argDecl.valueType)
        {
            case CommandLineArguments::ArgValType::String:
                parsedValue.asString = matchVal.first;
                return STATUS_OKAY;

            case CommandLineArguments::ArgValType::Integer:
                return ParseInteger(matchVal, parsedValue.asInteger);

            case CommandLineArguments::ArgValType::Float:
                return ParseFloat(matchVal, parsedValue.asFloat);

            case CommandLineArguments::ArgValType::EnumString:
            {
                /* if argument values is limited to enumerated values, there
                must be a configuration to provide the set of allowed values */
                _ASSERTE(argValCfg != nullptr);
                auto &typedArgValCfg = *static_cast<std::initializer_list<const char *> *> (argValCfg);
                ValidateEnumValue(argDecl, typedArgValCfg, matchVal.first);
                parsedValue.asString = matchVal.first;
                return STATUS_OKAY;
            }

            case CommandLineArguments::ArgValType::EnumInteger:
            {
                long long value;
                if (ParseInteger(matchVal, value) == STATUS_FAIL)
                    return STATUS_FAIL;

                /* if argument values is limited to enumerated values, there
                must be a configuration to provide the set of allowed values */
                _ASSERTE(argValCfg != nullptr);
                auto &typedArgValCfg = *static_cast<std::initializer_list<long long> *> (argValCfg);
                ValidateEnumValue(argDecl, typedArgValCfg, value);
                parsedValue.asInteger = value;
                return STATUS_OKAY;
            }

            case CommandLineArguments::ArgValType::RangeInteger:
            {
                long long value;
                if (ParseInteger(matchVal, value) == STATUS_FAIL)
                    return STATUS_FAIL;

                /* if argument expects value limited to a range, there
                must be a configuration to provide the range boundaries */
                _ASSERTE(argValCfg != nullptr);
                auto &typedArgValCfg = *static_cast<std::initializer_list<long long> *> (argValCfg);
                ValidateRangedValue(argDecl, typedArgValCfg, value);
                parsedValue.asInteger = value;
                return STATUS_OKAY;
            }

            case CommandLineArguments::ArgValType::RangeFloat:
            {
                double value;
                if (ParseFloat(matchVal, value) == STATUS_FAIL)
                    return STATUS_FAIL;

                /* if argument expects value limited to a range, there
                must be a configuration to provide the range boundaries */
                _ASSERTE(argValCfg != nullptr);
                auto &typedArgValCfg = *static_cast<std::initializer_list<double> *> (argValCfg);
                ValidateRangedValue(argDecl, typedArgValCfg, value);
                parsedValue.asFloat = value;
                return STATUS_OKAY;
            }

        default:
            _ASSERTE(false); // unexpected value type
            return STATUS_FAIL;
        }
    }

    /// <summary>
    /// Parses the arguments received from command line.
    /// </summary>
    /// <param name="argCount">The count of arguments.</param>
    /// <param name="arguments">The arguments.</param>
    /// <returns>
    /// <see cref="STATUS_OKAY"/> whenever successful, otherwise, <see cref="STATUS_FAIL"/>.
    /// </returns>
    bool CommandLineArguments::Parse(int argCount, const char *arguments[])
    {
        _ASSERTE(argCount > 1 && arguments != nullptr && arguments[argCount] == nullptr);

        CALL_STACK_TRACE;

        try
        {
            std::csub_match matchVal; // regex sub-match for value

            // iterator for the only allowed argument to be a value or list of values (if declared at all)
            auto valArgIter = (m_idValueTypeArg >= 0) ? m_expectedArgs.find(m_idValueTypeArg) : m_expectedArgs.end();

            // arguments that are options come with the value when the separator is not space
            bool optValueInSameArg(m_argValSeparator != ArgValSeparator::Space);

            std::ostringstream oss;
            oss << (m_useOptSignSlash ? '/' : '-') << R"(([a-z|\d]))";

            if (optValueInSameArg)
                oss << '(' << static_cast<char> (m_argValSeparator) << "(.+))?";

            // regex for option single char label (might be case sensitive)
            std::regex rgxOptCharLabel(oss.str().c_str(),
                std::regex_constants::ECMAScript | (m_isOptCaseSensitive ? 0 : std::regex_constants::icase)
            );

            oss.str("");

            if (m_useOptSignSlash)
                oss << R"(/([a-z|\d|_]{2,}))";
            else
                oss << R"(--([a-z|\d|-]{2,}))";

            if (optValueInSameArg)
                oss << '(' << static_cast<char> (m_argValSeparator) << "(.+))?";

            // regex for option name label
            std::regex rgxOptNameLabel(oss.str().c_str(),
                std::regex_constants::ECMAScript | std::regex_constants::icase
            );

            // Now parse the arguments:
            uint16_t idx(1);
            while (idx < argCount)
            {
                const char *arg = arguments[idx];

                uint16_t argId;
                std::cmatch match;

                // does the argument looks like an option with name label?
                if (std::regex_match(arg, match, rgxOptNameLabel))
                {
                    string optName = match[1].str();
                    auto iter = m_argsByNameLabel.find(optName);
                    if (m_argsByNameLabel.end() == iter)
                    {
                        std::cerr << "Parser error: command line option '" << optName << "' is unknown" << std::endl;
                        return STATUS_FAIL;
                    }

                    argId = iter->second;
                }
                // does the argument looks like an option with single char label?
                else if (std::regex_match(arg, match, rgxOptCharLabel))
                {
                    auto optChar = *(match[1].first);
                    auto iter = m_argsByCharLabel.find(optChar);
                    if (m_argsByCharLabel.end() == iter)
                    {
                        std::cerr << "Parser error: command line option '" << optChar << "' is unknown" << std::endl;
                        return STATUS_FAIL;
                    }

                    argId = iter->second;
                }
                else // is the argument a value or list of values?
                {
                    /* no argument which is a value or list of values has been declared
                    OR
                    there is one, but it is not a list, and more than one value has been caught here already */
                    if (valArgIter == m_expectedArgs.end()
                        || (valArgIter->second.common.type != ArgType::ValuesList
                            && m_parsedValArgs.size() != 0))
                    {
                        std::cerr << "Parser error: value '" << arg << "' was unexpected" << std::endl;
                        return STATUS_FAIL;
                    }

                    ParsedValue parsedValue;
                    matchVal.first = arg;
                    matchVal.second = arg + strlen(arg);
                    matchVal.matched = true;

                    if (ParseAndValidateValue(valArgIter->second.common,
                                              valArgIter->second.typedExtInfo,
                                              matchVal,
                                              parsedValue) == STATUS_FAIL)
                    {
                        return STATUS_FAIL;
                    }

                    m_parsedValArgs.push_back(parsedValue);
                    continue;
                }

                auto expArgIter = m_expectedArgs.find(argId);
                _ASSERTE(m_expectedArgs.end() != expArgIter);

                auto &expArg = expArgIter->second;
                auto &argDecl = expArg.common;

                // repeated option?
                auto parsedValIter = m_parsedOptVals.find(argId);
                if (m_parsedOptVals.end() != parsedValIter)
                {
                    std::cerr << "Parser error: command line option '" << match[1].str()
                              << "' appears more than once" << std::endl;

                    return STATUS_FAIL;
                }

                auto &parsedVal = parsedValIter->second;

                switch (argDecl.type)
                {
                case ArgType::OptionSwitch:

                    // switch-type option does not expect an accompanying value, but regex matched one:
                    if (optValueInSameArg && match[2].matched)
                    {
                        std::cerr << "Parser error: command line option '" << match[1].str()
                                  << "' does not expect an accompanying value" << std::endl;

                        return STATUS_FAIL;
                    }

                    parsedVal = { 0 };
                    break;

                case ArgType::OptionWithReqValue:

                    /* accompanying value should have appeared in this arg, but it didn't
                    OR
                    accompanying value should appear in the next arg, but there is none */
                    if ((optValueInSameArg && !match[2].matched)
                        || (!optValueInSameArg && (arg = arguments[++idx]) == nullptr))
                    {
                        std::cerr << "Parser error: command line option '" << match[1].str()
                                  << "' requires an accompanying value, but none has been specified" << std::endl;

                        return STATUS_FAIL;
                    }

                    if (optValueInSameArg)
                        matchVal = match[3];
                    else
                    {
                        matchVal.first = arg;
                        matchVal.second = arg + strlen(arg);
                        matchVal.matched = true;
                    }

                    if (ParseAndValidateValue(argDecl, expArg.typedExtInfo, matchVal, parsedVal) == STATUS_FAIL)
                        return STATUS_FAIL;

                    break;

                default:
                    _ASSERTE(false); // unexpected value type
                    return STATUS_FAIL;
                }

                m_parsedOptVals[argId] = parsedVal;
                ++idx;
            }// while loop end

            // an argument which was a list of values was expected:
            if (m_expectedArgs.end() != valArgIter
                && valArgIter->second.common.type == ArgType::ValuesList)
            {
                auto &argValCfg = *static_cast<std::initializer_list<uint16_t> *> (valArgIter->second.typedExtInfo);
                
                auto minCount = *argValCfg.begin();
                auto maxCount = *(argValCfg.begin() + 1);
                
                // wrong number of items?
                if (m_parsedValArgs.size() < minCount || m_parsedValArgs.size() > maxCount)
                {
                    std::cerr << "Parser error: list of values '" << valArgIter->second.common.optName << "' can only have ";

                    if (minCount != maxCount)
                        std::cerr << "from " << minCount << " to " << maxCount << " items";
                    else
                        std::cerr << minCount << " items" << std::endl;

                    return STATUS_FAIL;
                }
            }

            return STATUS_OKAY;
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when parsing command line arguments: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Tells whether a switch-type option argument was present in command line.
    /// </summary>
    /// <param name="id">The command line argument identifier.</param>
    /// <returns><c>true</c>, whenever the argument is present among the parsed ones, otherwise, <c>false</c>.</returns>
    bool CommandLineArguments::GetArgSwitchOptionValue(uint16_t id) const
    {
        CALL_STACK_TRACE;

        auto argDeclIter = m_expectedArgs.find(id);
        if (m_expectedArgs.end() == argDeclIter)
        {
            std::ostringstream oss;
            oss << "Cannot retrieve command line argument ID " << id << "because it has not been declared";
            throw AppException<std::invalid_argument>(oss.str());
        }

        _ASSERTE(argDeclIter->second.common.type == ArgType::OptionSwitch);

        auto parsedOptValsIter = m_parsedOptVals.find(id);
        if (m_parsedOptVals.end() != parsedOptValsIter)
            return true;

        return false;
    }

    /// <summary>
    /// Gets the string value for a command line argument option.
    /// </summary>
    /// <param name="id">The argument identifier.</param>
    /// <param name="isPresent">Tells whether the argument was present in the command line.</param>
    /// <returns>When not present in command line, the configured default. If no default
    /// has been configured, zero/null. When present, the provided accompanying value.</returns>
    const char * CommandLineArguments::GetArgOptionValueString(uint16_t id, bool &isPresent) const
    {
        CALL_STACK_TRACE;

        auto argDeclIter = m_expectedArgs.find(id);
        if (m_expectedArgs.end() == argDeclIter)
        {
            std::ostringstream oss;
            oss << "Cannot retrieve command line argument ID " << id << " because it has not been declared";
            throw AppException<std::invalid_argument>(oss.str());
        }
        
        auto extArgDecl = argDeclIter->second;

        // is this call ppropriate for the argument value type?
        _ASSERTE((static_cast<uint8_t> (extArgDecl.common.valueType) & static_cast<uint8_t> (ArgValType::String)) != 0
                 && (static_cast<uint8_t> (extArgDecl.common.type) & argIsOptionFlag) != 0);

        auto parsedOptValsIter = m_parsedOptVals.find(id);
        if (m_parsedOptVals.end() != parsedOptValsIter)
        {
            isPresent = true;
            return parsedOptValsIter->second.asString;
        }

        isPresent = false;
        auto argValCfg = static_cast<std::initializer_list<const char *> *> (extArgDecl.typedExtInfo);

        if (argValCfg != nullptr)
            return *(argValCfg->begin());
        else
            return nullptr;
    }
    
    /// <summary>
    /// Gets the integer value for a command line argument option.
    /// </summary>
    /// <param name="id">The argument identifier.</param>
    /// <param name="isPresent">Whether the argument was present in the command line.</param>
    /// <returns>When not present in command line, the configured default. If no default
    /// has been configured, zero/null. When present, the provided accompanying value.</returns>
    long long CommandLineArguments::GetArgOptionValueInteger(uint16_t id, bool &isPresent) const
    {
        CALL_STACK_TRACE;

        auto argDeclIter = m_expectedArgs.find(id);
        if (m_expectedArgs.end() == argDeclIter)
        {
            std::ostringstream oss;
            oss << "Cannot retrieve command line argument ID " << id << " because it has not been declared";
            throw AppException<std::invalid_argument>(oss.str());
        }

        auto extArgDecl = argDeclIter->second;
        
        // is this call ppropriate for the argument value type?
        _ASSERTE((static_cast<uint8_t> (extArgDecl.common.valueType) & static_cast<uint8_t> (ArgValType::Integer)) != 0
                 && (static_cast<uint8_t> (extArgDecl.common.type) & argIsOptionFlag) != 0);

        auto parsedOptValsIter = m_parsedOptVals.find(id);
        if (m_parsedOptVals.end() != parsedOptValsIter)
        {
            isPresent = true;
            return parsedOptValsIter->second.asInteger;
        }

        isPresent = false;
        auto argValCfg = static_cast<std::initializer_list<long long> *> (extArgDecl.typedExtInfo);

        // configuration of values is provided
        if (argValCfg != nullptr
            // there is a default value because it is limited to enumeration
            && ((static_cast<uint8_t> (extArgDecl.common.valueType) & static_cast<uint8_t> (argValIsEnumTypeFlag)) != 0
                // there is default value in configuration of range
                || (static_cast<uint8_t> (extArgDecl.common.valueType) & static_cast<uint8_t> (argValIsRangedTypeFlag)) != 0
                && argValCfg->size() > 2))
        {
            return *(argValCfg->begin());
        }
        else
            return 0;
    }

    /// <summary>
    /// Gets the floating point value for a command line argument option.
    /// </summary>
    /// <param name="id">The argument identifier.</param>
    /// <param name="isPresent">Whether the argument was present in the command line.</param>
    /// <returns>When not present in command line, the configured default. If no default
    /// has been configured, zero/null. When present, the provided accompanying value.</returns>
    double CommandLineArguments::GetArgOptionValueFloat(uint16_t id, bool &isPresent) const
    {
        CALL_STACK_TRACE;

        auto argDeclIter = m_expectedArgs.find(id);
        if (m_expectedArgs.end() == argDeclIter)
        {
            std::ostringstream oss;
            oss << "Cannot retrieve command line argument ID " << id << " because it has not been declared";
            throw AppException<std::invalid_argument>(oss.str());
        }

        auto extArgDecl = argDeclIter->second;

        // is this call ppropriate for the argument value type?
        _ASSERTE((static_cast<uint8_t> (extArgDecl.common.valueType) & static_cast<uint8_t> (ArgValType::Float)) != 0
                 && (static_cast<uint8_t> (extArgDecl.common.type) & argIsOptionFlag) != 0);

        auto parsedOptValsIter = m_parsedOptVals.find(id);
        if (m_parsedOptVals.end() != parsedOptValsIter)
        {
            isPresent = true;
            return parsedOptValsIter->second.asFloat;
        }

        isPresent = false;
        auto argValCfg = static_cast<std::initializer_list<double> *> (extArgDecl.typedExtInfo);

        // configuration of values is provided
        if (argValCfg != nullptr
            // there is a default value because it is limited to enumeration
            && ((static_cast<uint8_t> (extArgDecl.common.valueType) & static_cast<uint8_t> (argValIsEnumTypeFlag)) != 0
                // there is default value in configuration of range
                || (static_cast<uint8_t> (extArgDecl.common.valueType) & static_cast<uint8_t> (argValIsRangedTypeFlag)) != 0
                && argValCfg->size() > 2))
        {
            return *(argValCfg->begin());
        }
        else
            return 0;
    }

}// end of namespace core
}// end of namespace _3fd
