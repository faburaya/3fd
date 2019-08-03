#include "stdafx.h"
#include "xml.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace _3fd
{
namespace xml
{
    // selects the name substring out of XML content in buffer
    xstr GetNameSubstring(XmlBase xmlDocObj) noexcept
    {
        return xstr(xmlDocObj->name(), static_cast<uint32_t>(xmlDocObj->name_size()));
    }

    // selects the value substring out of XML content in buffer
    xstr GetValueSubstring(XmlBase xmlDocObj) noexcept
    {
        return xstr(xmlDocObj->value(), static_cast<uint32_t>(xmlDocObj->value_size()));
    }

    // makes a string to help indentation, given the desired count of spaces
    std::array<char, 256> GetIndentation(uint8_t indentation)
    {
        std::array<char, 256> spaces;
        memset(spaces.data(), ' ', indentation);
        spaces[indentation] = '\0';
        return spaces;
    }

    XmlConstValue<std::string> equal_to_copy_of(const char *str)
    {
        return XmlConstValue<std::string>(str);
    }

    /// <summary>
    /// Parses string value from another string.
    /// </summary>
    /// <param name="str">The string to parse.</param>
    /// <param name="to">The string to receive the parsed value.</param>
    /// <param name="format">The format the string has to obey, as a regex.</param>
    /// <returns>Whether the parsing has been successful.</returns>
    bool ParseValueFromString(const xstr &str,
                              std::string &to,
                              NoFormat)
    {
        to = std::string(str.begin(), str.end());
        return true;
    }
    
    /// <summary>
    /// Parses boolean value from string.
    /// </summary>
    /// <param name="str">The string to parse.</param>
    /// <param name="to">The boolean value to receive the parsed value.</param>
    /// <param name="format">The format the string has to obey.</param>
    /// <returns>Whether the parsing has been successful.</returns>
    bool ParseValueFromString(const xstr &str,
                              bool &to,
                              BooleanFormat format)
    {
        std::istrstream iss(str.data, str.lenBytes);

        if (format == BooleanFormat::Alpha)
            iss.setf(std::ios_base::boolalpha);
        else
            iss.unsetf(std::ios_base::boolalpha);

        return ParseValueFromStream(iss, to);
    }

    // parse string to boolean, format defaults to alpha
    bool ParseValueFromString(const xstr &str, bool &to, NoFormat)
    {
        return ParseValueFromString(str, to, BooleanFormat::Alpha);
    }

    /// <summary>
    /// Parses the XML document from a ready-only buffer.
    /// </summary>
    /// <param name="buffer">The buffer to read the XML content from.</param>
    /// <param name="dom">The XML document object to receive the parsed data.</param>
    /// <param name="root">The name of the root element.</param>
    /// <returns>The root XML element, or the first node in the document.</returns>
    rapidxml::xml_node<char> *ParseXmlFromBuffer(const char *buffer,
                                                 rapidxml::xml_document<char> &dom,
                                                 const char *root)
    {
        try
        {
            dom.parse<rapidxml::parse_non_destructive>(const_cast<char *>(buffer));
            return dom.first_node(root);
        }
        catch (rapidxml::parse_error &ex)
        {
            std::ostringstream oss;
            oss << "RAPIDXML parser error: " << ex.what() << " at " << ex.where<char>();
            throw core::AppException<std::runtime_error>("Failed to parse XML content from read-only buffer!", oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when parsing XML content from ready-only buffer: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Parses the XML document from a ready-only buffer.
    /// </summary>
    /// <param name="content">The string to read the XML content from.</param>
    /// <param name="dom">The XML document object to receive the parsed data.</param>
    /// <param name="root">The name of the root element.</param>
    /// <returns>The root XML element, or the first node in the document.</returns>
    rapidxml::xml_node<char> *ParseXmlFromString(const std::string &content,
                                                 rapidxml::xml_document<char> &dom,
                                                 const char *root)
    {
        return ParseXmlFromBuffer(content.c_str(), dom, root);
    }

    /// <summary>
    /// Parses the XML document from a file stream.
    /// </summary>
    /// <param name="input">The file stream.</param>
    /// <param name="buffer">The buffer to hold the file data.</param>
    /// <param name="dom">The XML document object to receive the parsed data.</param>
    /// <param name="root">The name of the root element.</param>
    /// <returns>The specified root XML element, or the first node in the document.</returns>
    rapidxml::xml_node<char> *ParseXmlFromStream(std::ifstream &input,
                                                 std::vector<char> &buffer,
                                                 rapidxml::xml_document<char> &dom,
                                                 const char *root)
    {
        buffer.clear();

        try
        {
            // get the file size:
            auto nBytes = static_cast<std::streamsize> (input.seekg(0, std::ios::end).tellg());
            input.seekg(0, std::ios::beg); // rewind

            // read the whole file at once:
            buffer.resize(nBytes + 1);
            input.read(buffer.data(), nBytes);
            nBytes = input.gcount(); // may differ due to char conversion when reading not in binary mode
            buffer[nBytes] = 0;
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when parsing XML content from file stream: "
                << core::StdLibExt::GetDetailsFromSystemError(ex);

            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when parsing XML content from file stream: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }

        if (input.bad())
            throw core::AppException<std::runtime_error>("Failure when reading XML from file stream!");

        return ParseXmlFromBuffer(buffer.data(), dom, root);
    }

    /// <summary>
    /// Parses the XML document from an input file.
    /// </summary>
    /// <param name="input">The path of the input file.</param>
    /// <param name="buffer">The buffer to hold the file data.</param>
    /// <param name="dom">The XML document object to receive the parsed data.</param>
    /// <param name="root">The name of the root element.</param>
    /// <returns>The specified root XML element, or the first node in the document.</returns>
    rapidxml::xml_node<char> *ParseXmlFromFile(const std::string &filePath,
                                               std::vector<char> &buffer,
                                               rapidxml::xml_document<char> &dom,
                                               const char *root)
    {
        try
        {
            std::ifstream ifs(filePath, std::ios::binary);

            if (!ifs.is_open())
                throw core::AppException<std::runtime_error>("Failed to open input file!", filePath);

            return ParseXmlFromStream(ifs, buffer, dom, root);
        }
        catch (core::IAppException &ex)
        {
            throw core::AppException<std::runtime_error>("Failed to parse XML file!", filePath, ex);
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when parsing XML content from input file: "
                << core::StdLibExt::GetDetailsFromSystemError(ex);

            throw core::AppException<std::runtime_error>(oss.str(), filePath);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when parsing XML content from input file: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str(), filePath);
        }
    }

    /// <summary>
    /// Creates a XML DOM subordinate query that checks whether the element matches a
    /// given name and, when a binding is provided, whether its value (if there is any)
    /// is successfully parsed according format constraint and is equal to one from
    /// the binding. 
    /// </summary>
    /// <param name="localName">The local name of the attribute.</param>
    /// <param name="enforcement">The type of enforcement for the query match (whether optional or required).</param>
    /// <param name="subQueries">A list of subordinate queries, which creates a
    /// hierarchycal structure that imitates the XML snippet one is searching for.</param>
    /// <param name="match">When provided, it receives the DOM element from the last successful match.</param>
    /// <return>The newly created subordinate query for XML element.</return>
    std::shared_ptr<XmlQueryNode> QueryElement(const char *localName,
                                               QueryMatchEnforcement enforcement,
                                               std::vector<std::shared_ptr<XmlQueryNode>> &&subQueries,
                                               const rapidxml::xml_node<char> **match)
    {
        return std::shared_ptr<XmlQueryNode>(
            new XmlQueryElement<XmlConstValue<void>, NoFormat>(localName,
                                                               XmlConstValue<void>(),
                                                               NoFormat(),
                                                               enforcement == Optional,
                                                               std::move(subQueries),
                                                               match)
        );
    }

    /// <summary>
    /// A  XML query is a tree with several nodes, and it can be serialized 
    /// into a textual description of what is being searched.
    /// Every node is responsible for serialization of its content, and
    /// this implementation outputs the ones that are attributes.
    /// </summary>
    /// <param name="name">The name of the attribute.</param>
    /// <param name="isOptional">Whether the match of the attribute is optional.</param>
    /// <param name="out">The output stream.</param>
    void SerializeXmlAttributeQueryTo(const std::string &name,
                                      bool isOptional,
                                      std::ostream &out)
    {
        out << (isOptional ? " *" : " ") << name << "=\"...\"";
    }

    /// <summary>
    /// A XML query is a tree with several nodes, and it can be serialized 
    /// into a textual description of what is being searched.
    /// Every node is responsible for serialization of its content, and
    /// this implementation outputs the ones that are elements.
    /// </summary>
    /// <param name="name">The name of the element.</param>
    /// <param name="isOptional">Whether the match of the element is optional.</param>
    /// <param name="subQueries">A list of the sub-queries under the element.</param>
    /// <param name="indentation">Count of spaces to indent this element.</param>
    /// <param name="out">The output stream.</param>
    void SerializeXmlElementQueryTo(const std::string &name,
                                    bool isOptional,
                                    const std::vector<std::shared_ptr<XmlQueryNode>> &subQueries,
                                    uint8_t indentation,
                                    std::ostream &out) noexcept
    {
        try
        {
            out << GetIndentation(indentation).data() << '<' << name;

            if (isOptional)
                out << '*';

            for (auto &subQuery : subQueries)
            {
                if (subQuery->GetType() == XmlQueryNode::NodeType::Attribute)
                    subQuery->SerializeTo(0, out);
            }

            if (!subQueries.empty())
            {
                out << ">\r\n";

                for (auto &subQuery : subQueries)
                {
                    if (subQuery->GetType() == XmlQueryNode::NodeType::Element)
                        subQuery->SerializeTo(indentation + 2, out);
                }
            }
            else
            {
                out << "...";
            }

            out << GetIndentation(indentation).data() << "</" << name << ">\r\n";
        }
        catch (std::exception &ex)
        {
            out << "Secondary failure during attempt to serialize XML query: " << ex.what() << "!\r\n";
        }
    }
    
    /// <summary>
    /// Checks all sub-queries in a XML element query.
    /// This implementation was moved here to be private, since it does not depend on template types.
    /// </summary>
    /// <param name="element">The XML element being tested by the query.</param>
    /// <param name="subQueries">The sub-queries.</param>
    /// <param name="nr">The XML name resolver.</param>
    /// <returns>Whether the overall result of checking all sub-queries was successful.</returns>
    bool CheckXmlElementSubQueries(XmlNode element,
                                   const std::vector<std::shared_ptr<XmlQueryNode>> &subQueries,
                                   const NamespaceResolver *nr)
    {
        for (auto &subQuery : subQueries)
        {
            switch (subQuery->GetType())
            {
            case XmlQueryNode::NodeType::Attribute:

                if (!subQuery->Check(GetFirstAttributeOf(element, subQuery->GetName(), nr), nr))
                    return false;

                break;

            case XmlQueryNode::NodeType::Element:
            {
                bool match;
                auto node = GetFirstChildNodeIn(element, subQuery->GetName(), nr);

                do
                {
                    if (node == nullptr)
                    {
                        if (!subQuery->IsOptional())
                            return false;

                        break;
                    }

                    match = (node->type() == rapidxml::node_element && subQuery->Check(node, nr));
                    node = GetNextSiblingOf(node, subQuery->GetName(), nr);

                } while (!match);

                break;
            }

            default:
                _ASSERTE(false); // unsupported node type!
                return false;
            }
        }

        return true;
    }

    /// <summary>
    /// Recursive implementation to execute a given query against the XML DOM tree.
    /// </summary>
    /// <param name="xmlDocNode">The XML DOM element.</param>
    /// <param name="strategy">How to search the XML DOM for a match.</param>
    /// <param name="nr">The resolver for XML names qualified by a namespace.</param>
    /// <returns>Whether a match has been found.</returns>
    bool ExecuteRecursiveImpl(XmlNode element,
                              XmlQueryNode *query,
                              QueryStrategy strategy,
                              const NamespaceResolver *nr)
    {
        if (element->type() != rapidxml::node_element)
            return false;

        if (IsNameEquivalent(element, query->GetName(), nr)
            && query->Check(element, nr))
        {
            return true;
        }

        if (strategy == QueryStrategy::TestsOnlyGivenElement)
            return false;

        auto node = GetFirstChildNodeIn(element, xstr(nullptr), nr);

        while (node != nullptr && node->type() == rapidxml::node_element)
        {
            if (ExecuteRecursiveImpl(node,
                                     query,
                                     QueryStrategy::TestsAllDescendantsRecursively,
                                     nr))
            {
                return true;
            }

            node = GetNextSiblingOf(node, xstr(nullptr), nr);
        }

        return false;
    }


    ////////////////////////////////////
    // Class NamespaceResolver
    ////////////////////////////////////

    // Normalizes URI for indexing
    static std::string GetNormalized(std::string uri)
    {
        // get rid of final backslash:
        if (uri.back() == '/')
            uri.pop_back();

        for (char &ch : uri)
            ch = tolower(ch);

        return uri;
    }
    
    /// <summary>
    /// Loads the declarations of namespaces from the provided element into the internal dictionary.
    /// </summary>
    /// <param name="element">The XML element whose attributes "xmlns:*" provide the declarations of namespace.</param>
    /// <returns>How many declarations of namespace have been loaded.</returns>
    uint32_t NamespaceResolver::LoadNamespacesFrom(XmlNode element)
    {
        if (element->type() != rapidxml::node_element)
        {
            throw core::AppException<std::logic_error>(
                "Resolver cannot load namespaces from a XML node that is not an element!",
                "XML node " + GetNameSubstring(element).to_string()
            );
        }

        int count(0);

        // iterates over all attributes looking for namespace declaration:
        for (auto attribute = element->first_attribute();
             attribute != nullptr;
             attribute = attribute->next_attribute())
        {
            char prefix[7];

            // try to parse namespace declaration:
            if (sscanf(attribute->name(), "xmlns:%6[^= ]", prefix) == 1)
            {
                char atEndOfPrefix = attribute->name()[strlen("xmlns:") + strlen(prefix)];

                // prefix too large?
                if (atEndOfPrefix != '=')
                {
                    std::ostringstream oss;
                    oss << "Resolver refuses XML namespace: prefix invalid or larger than "
                        << ((sizeof prefix) - 1)
                        << " characters!";

                    throw core::AppException<std::runtime_error>(
                        oss.str(),
                        GetNameSubstring(attribute).to_string() + " -> " + GetValueSubstring(attribute).to_string()
                    );
                }
            }
            // else, try to parse default namespace:
            else if (strncmp(attribute->name(), "xmlns=", 6) == 0)
            {
                prefix[0] = 0;
            }
            // not namespace related?
            else
                continue;

            auto attrValString = GetNormalized(GetValueSubstring(attribute).to_string());

            // add to the lookup dictionary
            if (!m_namespacesByPrefixInDoc.emplace(prefix, attrValString).second)
            {
                throw core::AppException<std::runtime_error>(
                    "Resolver detected repeated declaration of XML namespace prefix!",
                    std::string(prefix[0] != 0 ? prefix : "(default xmlns)")
                );
            }

            ++count;

            // add to the reverse lookup hash table
            m_prefixesByNamespace.emplace(attrValString, prefix);
        }

        return count;
    }
    
    /// <summary>
    /// Determines whether the given namespace has been loaded from document.
    /// </summary>
    /// <param name="nsUri">The URI of the namespace to look for.</param>
    /// <returns>
    ///   <c>true</c> if a namespace with the given URI has been loaded from document; otherwise, <c>false</c>.
    /// </returns>
    bool NamespaceResolver::Has(std::string nsUri) const
    {
        return m_prefixesByNamespace.find(GetNormalized(nsUri)) != m_prefixesByNamespace.end();
    }

    /// <summary>
    /// Parses a qualified XML name (from document, so a prefix would be acknowledged, whereas
    /// an alias for the same would not) into a pair of namespace (URI) + local name.
    /// </summary>
    /// <param name="name">The qualified name.</param>
    /// <param name="nsUri">The URI of the namespace in the qualified name.</param>
    /// <param name="localName">The local name of qualified name.</param>
    /// <returns>
    /// Whether the name has both a prefix for a namespace
    /// that is currently loaded and a local part.
    /// </returns>
    bool NamespaceResolver::ParseQualifiedName(const std::string &name,
                                               std::string &nsUri,
                                               std::string &localName)
    {
        auto delimPos = name.find(':');
        
        // no prefix means default namespace:
        if (delimPos == std::string::npos)
        {
            auto iter = m_namespacesByPrefixInDoc.find("");
            if (iter == m_namespacesByPrefixInDoc.end())
                return false;

            nsUri = iter->second;
            localName = name;
            return true;
        }

        localName = std::string(name.begin() + delimPos + 1, name.end());

        if (localName.empty())
            return false;

        std::string prefix = name.substr(0, delimPos);

        auto iter = m_namespacesByPrefixInDoc.find(prefix);

        if (iter != m_namespacesByPrefixInDoc.end())
        {
            nsUri = iter->second;
            return true;
        }

        return false;
    }

    /// <summary>
    /// Adds an alias for namespace prefix.
    /// Such an alias will be used used by users of this class to refer to the namespaces,
    /// because the user does not know beforehand what are the prefixes defined for them in the document.
    /// </summary>
    /// <param name="prefixAlias">The alias for the prefix.</param>
    /// <param name="ns">The referred namespace.</param>
    void NamespaceResolver::AddAliasForNsPrefix(const std::string &prefixAlias, const std::string &ns)
    {
        if (!m_namespacesByPrefixAlias.emplace(prefixAlias, ns).second)
        {
            throw core::AppException<std::logic_error>(
                "Resolver does not accept adding twice the same alias for XML namespace prefix!",
                prefixAlias + " -> " + ns);
        }
    }

    /// <summary>
    /// Gets a lists of equivalent names that come from translating a namespace aliases
    /// to the proper prefixes as declared in the XML document.
    /// </summary>
    /// <param name="name">The name of element or attribute, optionaly containing a namespace alias.</param>
    /// <returns>A list of equivalent names according to the translation of namespace alias to prefix.</returns>
    std::vector<std::string> NamespaceResolver::GetEquivalentNames(xstr qname) const
    {
        _ASSERTE(qname.data != nullptr);

        const char *endOfPrefix = strchr(qname.begin(), ':');

        // name is not qualified with a namespace?
        if (endOfPrefix == nullptr)
            return { qname.to_string() };

        std::string alias(qname.begin(), endOfPrefix);
        auto mapIter = m_namespacesByPrefixAlias.find(alias);

        // unknown alias?
        if (m_namespacesByPrefixAlias.end() == mapIter)
            return { qname.to_string() };

        const std::string &ns = mapIter->second;
        auto range = m_prefixesByNamespace.equal_range(ns);

        std::vector<std::string> result;
        result.reserve(std::distance(range.first, range.second));

        // iterate over all prefixes for such namespace:
        for (auto iter = range.first; iter != range.second; ++iter)
        {
            const std::string &prefix = iter->second;

            if (!prefix.empty())
                result.push_back(prefix + std::string(endOfPrefix, qname.end()));
            else
                result.push_back(std::string(endOfPrefix + 1, qname.end()));
        };

        return result;
    }
    
    /// <summary>
    /// Serializes the contents loaded into the XML namespace resolver into an output stream.
    /// </summary>
    /// <param name="indentation">Count of spaces to indent this element.</param>
    /// <param name="out">The output stream.</param>
    void NamespaceResolver::SerializeTo(uint8_t indentation, std::ostream &out) const noexcept
    {
        try
        {
            auto indentationString = GetIndentation(indentation);

            out << indentationString.data()
                << '[' << m_namespacesByPrefixInDoc.size()
                << " namespaces loaded from document]\r\n";

            for (auto &pair : m_namespacesByPrefixInDoc)
            {
                const std::string &prefix = pair.first;
                const std::string &ns = pair.second;

                out << indentationString.data() << prefix << " = " << ns << "\r\n";
            }

            out << indentationString.data()
                << '[' << m_namespacesByPrefixAlias.size()
                << " defined aliases for namespaces]\r\n";

            for (auto &pair : m_namespacesByPrefixAlias)
            {
                const std::string &prefix = pair.first;
                const std::string &ns = pair.second;

                out << indentationString.data() << prefix << " = " << ns << "\r\n";
            }
        }
        catch (std::exception &ex)
        {
            out << "Secondary failure during attempt to serialize content of resolver for XML namespaces: " << ex.what() << "!\r\n";
        }
    }
    
    /// <summary>
    /// Determines whether the XML node or attribute has the name equivalent to a given one.
    /// </summary>
    /// <param name="obj">The XML node or attribute whose name is to be tested.</param>
    /// <param name="qname">The expected name (optionally qualified by namespace alias).</param>
    /// <param name="nr">The resolver for XML names qualified by a namespace.</param>
    /// <returns>
    ///   <c>true</c> if the name & namespace is equivalent to the provided expectation; otherwise, <c>false</c>.
    /// </returns>
    bool IsNameEquivalent(XmlBase obj,
                          xstr qname,
                          const NamespaceResolver *nr)
    {
        if (nr == nullptr)
        {
            return strncmp(obj->name(), qname.data, qname.lenBytes) == 0
                && obj->name_size() == qname.lenBytes;
        }

        auto eqNames = nr->GetEquivalentNames(qname);

        for (const auto &name : eqNames)
        {
            if (name.compare(0, name.length(), obj->name(), obj->name_size()) == 0)
            {
                return true;
            }
        }

        return false;
    }

    template <typename XmlTypeOut, typename XmlPart>
    using XmlGetter = XmlTypeOut *(XmlPart::*)(const char *name, size_t size, bool caseSensitive) const;

    template <typename XmlTypeOut, typename XmlPart>
    XmlTypeOut *ResolveNameAndGet(const NamespaceResolver *resolver,
                                  XmlGetter<XmlTypeOut, XmlPart> get,
                                  const XmlPart *obj,
                                  xstr qname)
    {
        if (qname.null())
            return (obj->*get)(nullptr, 0, true);

        auto eqNames = resolver->GetEquivalentNames(qname);

        for (const auto &name : eqNames)
        {
            auto node = (obj->*get)(name.c_str(), name.length(), true);

            if (node != nullptr)
                return node;
        }

        return nullptr;
    }

    /// <summary>
    /// Gets the first child node of a XML element that matches the given name.
    /// If the name is qualified with a namespace alias, that is translated
    /// to its corresponding prefix loaded from the document.
    /// </summary>
    /// <param name="element">The XML element.</param>
    /// <param name="qname">Optional. The (qualified) name of the child node.</param>
    /// <param name="nr">The resolver for XML names qualified by a namespace.</param>
    /// <returns>The first child node that matches the name provided.</returns>
    XmlNode GetFirstChildNodeIn(XmlNode element,
                                xstr qname,
                                const NamespaceResolver *nr)
    {
        _ASSERTE(element->type() == rapidxml::node_element);

        if (nr == nullptr)
            return element->first_node(qname.data, qname.lenBytes);

        return ResolveNameAndGet(nr,
                                 &rapidxml::xml_node<char>::first_node,
                                 element,
                                 qname);
    }

    /// <summary>
    /// Gets the first attribute of a XML element that matches the given name.
    /// If the name is qualified with a namespace alias, that is translated
    /// to its corresponding prefix loaded from the document.
    /// </summary>
    /// <param name="element">The XML element.</param>
    /// <param name="qname">Optional. The (qualified) name of the attribute.</param>
    /// <param name="nr">The resolver for XML names qualified by a namespace.</param>
    /// <returns>The first attribute that matches the name provided.</returns>
    XmlAttribute GetFirstAttributeOf(XmlNode element,
                                     xstr qname,
                                     const NamespaceResolver *nr)
    {
        _ASSERTE(element->type() == rapidxml::node_element);

        if (nr == nullptr)
            return element->first_attribute(qname.data, qname.lenBytes);

        return ResolveNameAndGet(nr,
                                 &rapidxml::xml_node<char>::first_attribute,
                                 element,
                                 qname);
    }

    /// <summary>
    /// Gets the next sibling of a XML element that matches the given name.
    /// If the name is qualified with a namespace alias, that is translated
    /// to its corresponding prefix loaded from the document.
    /// </summary>
    /// <param name="element">The XML element.</param>
    /// <param name="qname">Optional. The (qualified) name of the sibling node.</param>
    /// <param name="nr">The resolver for XML names qualified by a namespace.</param>
    /// <returns>The first sibling node that matches the name provided.</returns>
    XmlNode GetNextSiblingOf(XmlNode element,
                             xstr qname,
                             const NamespaceResolver *nr)
    {
        _ASSERTE(element->type() == rapidxml::node_element);

        if (nr == nullptr)
            return element->next_sibling(qname.data, qname.lenBytes);

        return ResolveNameAndGet(nr,
                                 &rapidxml::xml_node<char>::next_sibling,
                                 element,
                                 qname);
    }

    /// <summary>
    /// Gets the next sibling of a XML element that matches the given name.
    /// If the name is qualified with a namespace alias, that is translated
    /// to its corresponding prefix loaded from the document.
    /// </summary>
    /// <param name="element">The XML element.</param>
    /// <param name="qname">Optional. The (qualified) name of the sibling node.</param>
    /// <param name="nr">The resolver for XML names qualified by a namespace.</param>
    /// <returns>The first sibling node that matches the name provided.</returns>
    XmlAttribute GetNextSiblingOf(XmlAttribute attribute,
                                  xstr qname,
                                  const NamespaceResolver *nr)
    {
        if (nr == nullptr)
            return attribute->next_attribute(qname.data, qname.lenBytes);

        return ResolveNameAndGet(nr,
                                 &rapidxml::xml_attribute<char>::next_attribute,
                                 attribute,
                                 qname);
    }

}// end of namespace xml
}// end of namespace _3fd
