//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#ifndef _3FD_XML_H // header guard
#define _3FD_XML_H

#include <3fd/core/exceptions.h>
#include <3fd/utils/text.h>
#include <rapidxml/rapidxml.hpp>

#include <array>
#include <algorithm>
#include <cinttypes>
#include <fstream>
#include <iomanip>
#include <map>
#include <strstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace _3fd
{
namespace xml
{
    using xstr = utils::CStringViewUtf8;

    typedef const rapidxml::xml_base<char> *XmlBase;

    typedef const rapidxml::xml_node<char> *XmlNode;

    typedef const rapidxml::xml_attribute<char> *XmlAttribute;

    //////////////////////////////////////////////
    // Helper for resolving XML namespaces
    //////////////////////////////////////////////

    /// <summary>
    /// Helps resolving prefixes to namespaces for XML doc browsing.
    /// The application parsing an XML document might know the namespaces URI's to be used
    /// in a XML document, but not the prefixes. This helper allows assignment of a known
    /// namespace URI to an alias. Then after parsing, one can find a qualified name for
    /// element or attribute ("nsPrefix:localName") using the known alias for the namespace
    /// instead of the unknown prefix.
    /// </summary>
    class NamespaceResolver
    {
    private:

        typedef std::map<std::string, std::string> LookupDictionary;

        LookupDictionary m_namespacesByPrefixInDoc;

        LookupDictionary m_namespacesByPrefixAlias;

        typedef std::unordered_multimap<std::string, std::string> ReverseLookupHashTable;

        ReverseLookupHashTable m_prefixesByNamespace;

    public:

        void AddAliasForNsPrefix(const std::string &prefixAlias, const std::string &ns);

        uint32_t LoadNamespacesFrom(XmlNode element);

        bool Has(const std::string &nsUri) const;

        bool ParseQualifiedName(const std::string &name,
                                std::string &nsUri,
                                std::string &localName);

        std::vector<std::string> GetEquivalentNames(xstr qname) const;

        void SerializeTo(uint8_t indentation, std::ostream &out) const noexcept;
    };

    bool IsNameEquivalent(XmlBase obj,
                          xstr qname = xstr(nullptr),
                          const NamespaceResolver *nr = nullptr);

    XmlNode GetFirstChildNodeIn(XmlNode element,
                                xstr qname = xstr(nullptr),
                                const NamespaceResolver *nr = nullptr);

    XmlAttribute GetFirstAttributeOf(XmlNode element,
                                     xstr qname = xstr(nullptr),
                                     const NamespaceResolver *nr = nullptr);

    XmlNode GetNextSiblingOf(XmlNode element,
                             xstr qname = xstr(nullptr),
                             const NamespaceResolver *nr = nullptr);

    XmlAttribute GetNextSiblingOf(XmlAttribute attribute,
                                  xstr qname = xstr(nullptr),
                                  const NamespaceResolver *nr = nullptr);

    //////////////////////////////////////////////
    // Helpers for parsing values from XML DOM
    //////////////////////////////////////////////

    xstr GetNameSubstring(XmlBase xmlDocObj) noexcept;

    xstr GetValueSubstring(XmlBase xmlDocObj) noexcept;

    // empty type to represent no format
    struct NoFormat {};

    template <typename ValType>
    bool ParseValueFromStream(std::istrstream &iss, ValType &to)
    {
        iss >> to;
        return !iss.fail();
    }

    // fallback, in case no other specific overload is a match
    template <typename ValType>
    bool ParseValueFromString(const xstr &str, ValType &to, NoFormat)
    {
        std::istrstream iss(str.data, str.lenBytes);
        return ParseValueFromStream(iss, to);
    }

    // copy string to an STL array
    template <size_t t_arraySize>
    bool ParseValueFromString(const xstr &str,
                              std::array<char, t_arraySize> &to,
                              NoFormat)
    {
        int rc = snprintf(to.data(), to.size(), "%.*s", str.lenBytes, str.data);
        return rc >= 0 && rc < to.size();
    }

    bool ParseValueFromString(const xstr &str, std::string &to, NoFormat);

    enum class BooleanFormat : uint8_t { Alpha, Numeric };

    bool ParseValueFromString(const xstr &str, bool &to, BooleanFormat format);

    bool ParseValueFromString(const xstr &str, bool &to, NoFormat);

    enum class IntegerFormat : uint8_t { Decimal, Hexa, Octal };

    // parse all types of integer value
    template <typename IntType>
    bool ParseValueFromString(const xstr &str,
                              IntType &to,
                              IntegerFormat format)
    {
        static_assert(std::is_integral<IntType>::value,
                      "this overload is meant only for integral types!");

        std::istrstream iss(str.data, str.lenBytes);

        switch (format)
        {
        case IntegerFormat::Decimal:
            iss.setf(std::ios_base::dec, std::ios_base::basefield);
            break;
        case IntegerFormat::Hexa:
            iss.setf(std::ios_base::hex, std::ios_base::basefield);
            break;
        case IntegerFormat::Octal:
            iss.setf(std::ios_base::oct, std::ios_base::basefield);
            break;
        default:
            break;
        }

        return ParseValueFromStream(iss, to);
    }

    enum class FloatFormat : uint8_t { Fixed, Scientific, Hexa };

    // parse floating point value (single or double precision)
    template <typename FloatType>
    bool ParseValueFromString(const xstr &str,
                              FloatType &to,
                              FloatFormat format)
    {
        static_assert(std::is_floating_point<FloatType>::value,
                      "this overload is meant only for floating point types!");

        std::istrstream iss(str.data, str.lenBytes);

        switch (format)
        {
        case FloatFormat::Fixed:
            iss.setf(std::ios_base::fixed, std::ios_base::floatfield);
            break;
        case FloatFormat::Scientific:
            iss.setf(std::ios_base::scientific, std::ios_base::floatfield);
            break;
        case FloatFormat::Hexa:
            iss.setf(std::ios_base::fixed | std::ios_base::scientific, std::ios_base::floatfield);
            break;
        default:
            break;
        }

        return ParseValueFromStream(iss, to);
    }

    //////////////////////////////
    // Value Wrappers & Helpers
    //////////////////////////////

    /// <summary>
    /// Holds a value that will be set by XML parsing.
    /// </summary>
    template <typename Type>
    struct XmlValue
    {
        Type &value;

        explicit XmlValue(Type &p_value) noexcept
            : value(p_value) {}
    };

    template <> struct XmlValue<void> {};

    // hold the reference to receive a parsed value
    template <typename Type>
    XmlValue<typename std::remove_reference<Type>::type> parse_into(Type &var) noexcept
    {
        static_assert(!std::is_const<Type>::value, "cannot parse into constant value!");
        static_assert(!std::is_same<const Type, char * const>::value, "no support for reference to C-style string!");
        static_assert(!std::is_same<const Type, const char * const>::value, "no support for reference to C-style string!");

        return XmlValue<typename std::remove_reference<Type>::type>(var);
    }

    /// <summary>
    /// Holds a value that will serve as a constraint during search on the XML DOM.
    /// </summary>
    template <typename Type>
    struct XmlConstValue
    {
        const Type value;

        explicit XmlConstValue(const Type &p_value) noexcept
            : value(p_value) {}
    };

    template <> struct XmlConstValue<void> {};

    XmlConstValue<std::string> equal_to_copy_of(const char *str);

    // make a copy of the referenced value to use it as a value constraint
    template <typename Type>
    XmlConstValue<Type> equal_to_copy_of(const Type &ref) noexcept
    {
        return XmlConstValue<Type>(ref);
    }

    // forbid to holding a reference to an r-value
    template <typename Type>
    XmlConstValue<Type> equal_to_ref_of(Type &&ref) noexcept
    {
        static_assert(std::is_reference<Type>::value, "cannot hold reference to r-value!");
        static_assert(!std::is_same<const Type, char * const>::value, "no support for reference to C-style string!");
        static_assert(!std::is_same<const Type, const char * const>::value, "no support for reference to C-style string!");

        return XmlConstValue<Type>(ref);
    }

    //////////////////////////////
    // XML DOM parts
    //////////////////////////////
    
    /// <summary>
    /// Enumerates options of how to search the XML DOM to match a query for an element.
    /// </summary>
    enum class QueryStrategy : uint8_t
    {
        TestsOnlyGivenElement,
        TestsAllDescendantsRecursively
    };

    /// <summary>
    /// Base class for every object that helps testing constraints for
    /// or parsing values from XML DOM nodes. A DOM query can be represented
    /// by several of these objects connected as a treee that reflects the
    /// XML structure the query is looking for.
    /// </summary>
    class XmlQueryNode
    {
    private:

        xstr m_localName;

        const bool m_isOptional;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="XmlQueryNode" /> class.
        /// </summary>
        /// <param name="localName">The local name of the XML node.</param>
        /// <param name="isOptional">if set to <c>true</c>, the failure to find a match
        /// for this query node does not compromise the whole query downwards.</param>
        XmlQueryNode(const char *localName, bool isOptional) noexcept
            : m_localName(localName)
            , m_isOptional(isOptional)
        {}

        virtual ~XmlQueryNode() {}

        /// <summary>
        /// Gets the local name of the XML node.
        /// </summary>
        /// <returns>The name of the XML node without namespace resolution.</returns>
        xstr GetName() const noexcept
        {
            return m_localName;
        }

        /// <summary>
        /// Determines whether the match of this query node is optional.
        /// </summary>
        /// <returns>
        ///   <c>true</c> when no match is found for this query node does not compromise the success of the whole query downwards.
        /// </returns>
        bool IsOptional() const noexcept
        {
            return m_isOptional;
        }

        enum class NodeType { Attribute, Element };

        virtual NodeType GetType() const noexcept = 0;

        virtual bool Check(XmlBase xmlObject, const NamespaceResolver *nr) = 0;

        virtual bool Execute(XmlNode element,
                             QueryStrategy mode,
                             const NamespaceResolver *nr = nullptr) = 0;

        virtual void SerializeTo(uint8_t indentation, std::ostream &out) const = 0;
    };

    // checks whether attribute/element value is successfully parsed to the binding
    template <typename ValType, typename FormatType>
    bool CheckParse(XmlBase xmlDocObj,
                    XmlValue<ValType> &binding,
                    const FormatType &valFormat)
    {
        
        return ParseValueFromString(GetValueSubstring(xmlDocObj), binding.value, valFormat);
    }

    // when no binding is provided, parsing is skipped
    template <typename FormatType>
    bool CheckParse(XmlBase ,
                    const XmlValue<void> &,
                    const FormatType &)
    {
        return true;
    }

    // checks whether the attribute/element value can be successfully parsed and matches the one from the binding
    template <typename ValType, typename FormatType>
    bool CheckParse(XmlBase xmlDocObj,
                    const XmlConstValue<ValType> &binding,
                    const FormatType &valFormat)
    {
        typename std::remove_reference<ValType>::type value;

        if (ParseValueFromString(GetValueSubstring(xmlDocObj), value, valFormat))
        {
            return value == binding.value;
        }
        
        return false;
    }

    // when no binding is provided, parsing is skipped
    template <typename FormatType>
    bool CheckParse(XmlBase ,
                    const XmlConstValue<void> &,
                    const FormatType &)
    {
        return true;
    }

    // private implementation for XmlQueryAttribute::SerializeTo
    void SerializeXmlAttributeQueryTo(const std::string &name,
                                      bool isOptional,
                                      std::ostream &out);

    /// <summary>
    /// Represents a XML attribute for DOM query whose value must be parsed.
    /// </summary>
    /// <seealso cref="XmlQueryNode" />
    template <typename XmlValType, typename FormatType>
    class XmlQueryAttribute : public XmlQueryNode
    {
    private:

        FormatType m_format;

        XmlValType m_binding;

        /// <summary>
        /// Checks whether a given XML attribute complies with this constraint, which
        /// includes an attempt to parse its string value into the expected type.
        /// </summary>
        /// <param name="attribute">The XML DOM attribute.</param>
        /// <param name="nr">The resolver for XML names qualified by a namespace.</param>
        /// <returns>Whether the XML attribute value was successfully parsed given the format constraint and,
        /// depending on the purpose of this instance, if such value matches the one from the binding. When
        /// this match is optional, the check always succeeds.</returns>
        bool Check(XmlBase attribute, const NamespaceResolver *nr) override
        {
            if (attribute == nullptr)
                return IsOptional();

            _ASSERTE(IsNameEquivalent(attribute, GetName(), nr));

            return CheckParse(attribute, m_binding, m_format) || IsOptional();
        }

        // This method is not supposed to be invoked by this instance
        bool Execute(XmlNode,
                     QueryStrategy,
                     const NamespaceResolver *) override
        {
            _ASSERTE(false);
            return false;
        }
        
        /// <summary>
        /// A query is a tree with several nodes, and it can be serialized 
        /// into a textual description of what is being searched.
        /// Every node is responsible for serialization of its content, hence
        /// this implementation outputs an attribute.
        /// </summary>
        /// <param name="out">The output stream.</param>
        void SerializeTo(uint8_t, std::ostream &out) const override
        {
            SerializeXmlAttributeQueryTo(GetName().to_string(), IsOptional(), out);
        }

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="XmlQueryAttribute"/> class.
        /// </summary>
        /// <param name="localName">The local name of the XML attribute.</param>
        /// <param name="binding">A binding to an external variable that is either to receive
        /// the value parsed from the XML attribute, or to hold a value constraint for it.</param>
        /// <param name="format">A format constraint for the attribute value.</param>
        /// <param name="isOptional">if set to <c>true</c>, the failure to find a match
        /// for this query node does not compromise the whole query downwards.</param>
        XmlQueryAttribute(const char *localName,
                          XmlValType &&binding,
                          FormatType &&format,
                          bool isOptional) noexcept
            : XmlQueryNode(localName, isOptional)
            , m_binding(std::move(binding))
            , m_format(std::move(format))
        {}

        /// <summary>
        /// Initializes a new instance of the <see cref="XmlQueryAttribute"/>
        /// class using move semantics.
        /// </summary>
        /// <param name="ob">The object whose resources are to be stolen.</param>
        XmlQueryAttribute(XmlQueryAttribute &&ob) noexcept
            : XmlQueryNode(ob.GetName(), ob.IsOptional())
            , m_binding(std::move(ob.m_binding))
            , m_format(std::move(ob.m_format))
        {}

        virtual ~XmlQueryAttribute() {};
        
        /// <summary>
        /// Gets the type of query node.
        /// </summary>
        /// <returns>Type of this query node.</returns>
        NodeType GetType() const noexcept override
        {
            return NodeType::Attribute;
        }
    };

    /// <summary>
    /// Creates a XML DOM subordinate query that checks whether the attribute matches
    /// a given name and, when a binding is provided, if its value is successfully
    /// parsed according format constraint and is equal to one from the binding.
    /// </summary>
    /// <param name="localName">The local name of the attribute.</param>
    /// <param name="binding">Optional binding to a variable set with the expected value.</param>
    /// <param name="format">The format for the attribute value.</param>
    template <typename ValType = void, typename FormatType = NoFormat>
    std::shared_ptr<XmlQueryNode> QueryAttribute(const char *localName,
                                                 XmlConstValue<ValType> &&binding = XmlConstValue<void>(),
                                                 FormatType &&valFormat = FormatType())
    {
        static_assert(!std::is_void<ValType>::value || std::is_same<FormatType, NoFormat>::value,
                      "Cannot specify format unless you have a non-void binding!");

        return std::shared_ptr<XmlQueryNode>(
            dbg_new XmlQueryAttribute<XmlConstValue<ValType>, FormatType>(localName,
                                                                      std::move(binding),
                                                                      std::move(valFormat),
                                                                      false)
        );
    }

    /// <summary>
    /// Enumerates the options for enforcement of query match.
    /// </summary>
    enum QueryMatchEnforcement
    {
        Required, // match of query is required, query fails otherwise
        Optional // match is optional, query succeeds disregarding
    };

    /// <summary>
    /// Creates a XML DOM subordinate query that checks whether the attribute matches
    /// a given name and its value is successfully parsed according format constraint.
    /// </summary>
    /// <param name="localName">The local name of the attribute.</param>
    /// <param name="enforcement">The type of enforcement for the query match (whether optional or required).</param>
    /// <param name="binding">Optional binding to a variable to receive the parsed value.</param>
    /// <param name="format">The format for the attribute value.</param>
    /// <return>The newly created subordinate query for XML attribute.</return>
    template <typename ValType, typename FormatType = NoFormat>
    std::shared_ptr<XmlQueryNode> QueryAttribute(const char *localName,
                                                 QueryMatchEnforcement enforcement,
                                                 XmlValue<ValType> &&binding,
                                                 FormatType &&valFormat = FormatType())
    {
        static_assert(!std::is_void<ValType>::value || std::is_same<FormatType, NoFormat>::value,
                      "Cannot specify format unless you have a non-void binding!");

        return std::shared_ptr<XmlQueryNode>(
            dbg_new XmlQueryAttribute<XmlValue<ValType>, FormatType>(localName,
                                                                 std::move(binding),
                                                                 std::move(valFormat),
                                                                 enforcement == Optional)
        );
    }

    // private implementation for XmlQueryElement::SerializeTo
    void SerializeXmlElementQueryTo(const std::string &name,
                                    bool isOptional,
                                    const std::vector<std::shared_ptr<XmlQueryNode>> &subQueries,
                                    uint8_t indentation,
                                    std::ostream &out) noexcept;

    // private implementation for XmlQueryElement::Check
    bool CheckXmlElementSubQueries(XmlNode element,
                                   const std::vector<std::shared_ptr<XmlQueryNode>> &subQueries,
                                   const NamespaceResolver *nr);

    // private implementation for XmlQueryElement::Execute
    bool ExecuteRecursiveImpl(XmlNode element,
                              XmlQueryNode *query,
                              QueryStrategy strategy,
                              const NamespaceResolver *nr);

    /// <summary>
    /// Represents a XML element for DOM query.
    /// </summary>
    /// <seealso cref="XmlQueryNode" />
    template <typename XmlValType, typename FormatType>
    class XmlQueryElement : public XmlQueryNode
    {
    private:

        std::vector<std::shared_ptr<XmlQueryNode>> m_subQueries;

        XmlNode *m_lastMatch;

        FormatType m_format;

        XmlValType m_binding;

        /// <summary>
        /// Checks whether a given XML element complies with this query.
        /// </summary>
        /// <param name="xmlDocObj">The XML element from DOM.</param>
        /// <param name="nr">The resolver for XML names qualified by a namespace.</param>
        /// <returns>Whether the given element obeys the constraint enforced by this instance,
        /// as well as the sub-queries and the corresponding nodes down the DOM tree.</returns>
        bool Check(XmlBase xmlDocObj, const NamespaceResolver *nr) override
        {
            auto element = static_cast<XmlNode>(xmlDocObj);

            _ASSERTE(element->type() == rapidxml::node_element);

            _ASSERTE(IsNameEquivalent(element, GetName(), nr));

            if (!CheckParse(element, m_binding, m_format))
                return this->IsOptional();

            if (!CheckXmlElementSubQueries(element, m_subQueries, nr))
                return false;

            if (m_lastMatch != nullptr)
                *m_lastMatch = element;

            return true;
        }

        /// <summary>
        /// A query is a tree with several nodes, and it can be serialized 
        /// into a textual description of what is being searched.
        /// Every node is responsible for serialization of its content, hence
        /// this implementation outputs an element.
        /// </summary>
        /// <param name="indentation">Count of spaces to indent this element.</param>
        /// <param name="out">The output stream.</param>
        void SerializeTo(uint8_t indentation, std::ostream &out) const override
        {
            SerializeXmlElementQueryTo(GetName().to_string(),
                                       IsOptional(),
                                       m_subQueries,
                                       indentation,
                                       out);
        }

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="XmlQueryElement" /> class.
        /// </summary>
        /// <param name="localName">The local name of the XML element.</param>
        /// <param name="binding">An optional binding to a variable.</param>
        /// <param name="format">The format for the element value.</param>
        /// <param name="isOptional">if set to <c>true</c>, the failure to find a match
        /// for this query node does not compromise the whole query downwards.</param>
        /// <param name="subQueries">A list of subordinate queries, which creates a
        /// hierarchycal structure that imitates the XML snippet one is searching for.</param>
        /// <param name="match">When provided, it receives the DOM element from the last successful match.</param>
        XmlQueryElement(const char *localName,
                        XmlValType &&binding,
                        FormatType &&format,
                        bool isOptional,
                        std::vector<std::shared_ptr<XmlQueryNode>> &&subQueries,
                        XmlNode *match) noexcept
            : XmlQueryNode(localName, isOptional)
            , m_subQueries(std::move(subQueries))
            , m_lastMatch(match)
            , m_binding(std::move(binding))
            , m_format(std::move(format))
        {}

        /// <summary>
        /// Initializes a new instance of the <see cref="XmlQueryElement"/> 
        /// class using move semantics.
        /// </summary>
        /// <param name="ob">The object whose resources will be stolen from.</param>
        XmlQueryElement(XmlQueryElement &&ob) noexcept
            : XmlQueryNode(ob.localName(), ob.IsOptional())
            , m_subQueries(std::move(ob.m_subQueries))
            , m_lastMatch(ob.m_lastMatch)
            , m_binding(std::move(ob.m_binding))
            , m_format(std::move(ob.m_format))
        {
            ob.m_lastMatch = nullptr;
        }
        
        virtual ~XmlQueryElement() {}

        /// <summary>
        /// Gets the type of query node.
        /// </summary>
        /// <returns>Type of this query node.</returns>
        NodeType GetType() const noexcept override
        {
            return NodeType::Element;
        }

        /// <summary>
        /// Executes this query on the given XML DOM element.
        /// </summary>
        /// <param name="xmlDocNode">The XML DOM element.</param>
        /// <param name="strategy">How to search the XML DOM for a match.</param>
        /// <param name="nr">The resolver for XML names qualified by a namespace.</param>
        /// <returns><c>true</c> when this query is optional or when this query is
        /// required and a match has been found, otherwise, <c>false</c>.</returns>
        bool Execute(XmlNode element,
                     QueryStrategy strategy,
                     const NamespaceResolver *nr) override
        {
            if (ExecuteRecursiveImpl(element, this, strategy, nr))
                return true;

            return IsOptional();
        }
    };

    std::shared_ptr<XmlQueryNode> QueryElement(const char *localName,
                                               QueryMatchEnforcement enforcement,
                                               std::vector<std::shared_ptr<XmlQueryNode>> &&subQueries = {},
                                               XmlNode *match = nullptr);

    /// <summary>
    /// Creates a XML DOM subordinate query that checks whether the element matches a
    /// given name and, when a binding is provided, whether its value (if there is any)
    /// is successfully parsed according format constraint and is equal to one from
    /// the binding. 
    /// </summary>
    /// <param name="localName">The local name of the attribute.</param>
    /// <param name="enforcement">The type of enforcement for the query match (whether optional or required).</param>
    /// <param name="binding">A binding to a variable set with the expected value.</param>
    /// <param name="format">The format for the attribute value.</param>
    /// <param name="subQueries">A list of subordinate queries, which creates a
    /// hierarchycal structure that imitates the XML snippet one is searching for.</param>
    /// <param name="match">When provided, it receives the DOM element from the last successful match.</param>
    /// <return>The newly created subordinate query for XML element.</return>
    template <typename ValType, typename FormatType = NoFormat>
    std::shared_ptr<XmlQueryNode> QueryElement(const char *localName,
                                               QueryMatchEnforcement enforcement,
                                               XmlConstValue<ValType> &&binding,
                                               FormatType &&valFormat = FormatType(),
                                               std::vector<std::shared_ptr<XmlQueryNode>> &&subQueries = {},
                                               XmlNode *match = nullptr)
    {
        static_assert(!std::is_void<ValType>::value || std::is_same<FormatType, NoFormat>::value,
                      "Cannot specify format unless you have a non-void binding!");

        // A well formed XML document cannot have an element with both a value and child elements!
        _ASSERTE(std::is_void<ValType>::value ||
            std::none_of(subQueries.begin(), subQueries.end(),
                [](const std::shared_ptr<XmlQueryNode> &subQuery)
                {
                    return subQuery->GetType() == XmlQueryNode::NodeType::Element;
                })
        );

        return std::shared_ptr<XmlQueryNode>(
            dbg_new XmlQueryElement<XmlConstValue<ValType>, FormatType>(localName,
                                                                    std::move(binding),
                                                                    std::move(valFormat),
                                                                    enforcement == Optional,
                                                                    std::move(subQueries),
                                                                    match)
        );
    }

    /// <summary>
    /// Creates a XML DOM subordinate query that checks whether the element matches
    /// a given name and its value is successfully parsed according format constraint.
    /// </summary>
    /// <param name="localName">The local name of the attribute.</param>
    /// <param name="enforcement">The type of enforcement for the query match (whether optional or required).</param>
    /// <param name="binding">A binding to a variable to receive the parsed value.</param>
    /// <param name="format">The format for the attribute value.</param>
    /// <param name="subQueries">A list of subordinate queries, which creates a
    /// hierarchycal structure that imitates the XML snippet one is searching for.</param>
    /// <param name="match">When provided, it receives the DOM element from the last successful match.</param>
    /// <return>The newly created subordinate query for XML element.</return>
    template <typename ValType, typename FormatType = NoFormat>
    std::shared_ptr<XmlQueryNode> QueryElement(const char *localName,
                                               QueryMatchEnforcement enforcement,
                                               XmlValue<ValType> &&binding,
                                               FormatType &&valFormat = FormatType(),
                                               std::vector<std::shared_ptr<XmlQueryNode>> &&subQueries = {},
                                               XmlNode *match = nullptr)
    {
        static_assert(!std::is_void<ValType>::value || std::is_same<FormatType, NoFormat>::value,
                      "Cannot specify format unless you have a non-void binding!");

        // A well formed XML document cannot have an element with both a value and child elements!
        _ASSERTE(std::is_void<ValType>::value ||
            std::count_if(subQueries.begin(), subQueries.end(),
                [](const std::shared_ptr<XmlQueryNode> &subQuery)
                {
                    return subQuery->GetType() == XmlQueryNode::NodeType::Element;
                }) == 0
        );

        return std::shared_ptr<XmlQueryNode>(
            dbg_new XmlQueryElement<XmlValue<ValType>, FormatType>(localName,
                                                               std::move(binding),
                                                               std::move(valFormat),
                                                               enforcement == Optional,
                                                               std::move(subQueries),
                                                               match)
        );
    }


    //////////////////////////////////
    // Helpers for load & parse
    //////////////////////////////////

    rapidxml::xml_node<char> *ParseXmlFromBuffer(const char *buffer,
                                                 rapidxml::xml_document<char> &dom,
                                                 const char *root = nullptr);

    rapidxml::xml_node<char> *ParseXmlFromString(const std::string &content,
                                                 rapidxml::xml_document<char> &dom,
                                                 const char *root = nullptr);

    rapidxml::xml_node<char> *ParseXmlFromStream(std::ifstream &input,
                                                 std::vector<char> &buffer,
                                                 rapidxml::xml_document<char> &dom,
                                                 const char *root = nullptr);

    rapidxml::xml_node<char> *ParseXmlFromFile(const std::string &filePath,
                                               std::vector<char> &buffer,
                                               rapidxml::xml_document<char> &dom,
                                               const char *root = nullptr);

}// end of namespace xml
}// end of namespace _3fd

#endif // end of header guard
