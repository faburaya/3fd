//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include <3fd/utils/xml.h>
#include <vector>
#include <string>

#ifdef _3FD_PLATFORM_WINRT
#   include <3fd/utils/winrt.h>
#endif

namespace _3fd
{
namespace unit_tests
{
    const char * const xmlContent = R"(
        <?xml version="1.0" encoding="utf-8"?>
        <einstellungen>
            <sprache>Deutsch</sprache>
            <version>6.96</version>
            <tiere>
                <eintrag schluessel="Hund" wert="Au Au" />
                <eintrag schluessel="Katze" wert="Miau" />
            </tiere>
            <wagen>
                <eintrag schluessel="Volkswagen" wert="17000" />
                <eintrag schluessel="Hyundai" wert="20300" />
            </wagen>
            <lebensmittel quality="true">
                <eintrag schluessel="Champignon" wert="3.50" />
                <eintrag schluessel="Kohlrabi" wert="2.50" />
            </lebensmittel>
        </einstellungen>
    )";

    static std::string GetXmlNameSubstring(const rapidxml::xml_base<char> *obj)
    {
        return std::string(obj->name(), obj->name() + obj->name_size());
    }

    /// <summary>
    /// Tests parsing XML content from buffer.
    /// </summary>
    TEST(Framework_XmlParser_TestCase, Parse_Buffer_Test)
    {
        rapidxml::xml_document<char> dom;

        auto root = xml::ParseXmlFromBuffer(xmlContent, dom, "einstellungen");

        ASSERT_TRUE(root != nullptr);
        EXPECT_TRUE(GetXmlNameSubstring(root) == "einstellungen");
    }

    /// <summary>
    /// Tests parsing XML content from a string.
    /// </summary>
    TEST(Framework_XmlParser_TestCase, Parse_String_Test)
    {
        rapidxml::xml_document<char> dom;

        std::string text(xmlContent);
        auto root = xml::ParseXmlFromString(text, dom, "einstellungen");

        ASSERT_TRUE(root != nullptr);
        EXPECT_TRUE(GetXmlNameSubstring(root) == "einstellungen");
    }

    /// <summary>
    /// Tests parsing XML content from a file stream.
    /// </summary>
    TEST(Framework_XmlParser_TestCase, Parse_Stream_Test)
    {
#ifndef _3FD_PLATFORM_WINRT
        const std::string filePath("_dummy.xml");
#else
        const std::string filePath = utils::WinRTExt::GetFilePathUtf8(
            "_dummy.xml",
            utils::WinRTExt::FileLocation::LocalFolder
        );
#endif
        std::ofstream ofs;
        ofs.open(filePath, std::ios_base::trunc);
        ASSERT_TRUE(ofs.is_open());
        ofs.write(xmlContent, strlen(xmlContent));
        ASSERT_FALSE(ofs.bad());
        ofs.close();

        std::ifstream ifs;
        ifs.open(filePath, std::ios_base::binary);
        ASSERT_TRUE(ifs.is_open());

        std::vector<char> buffer;
        rapidxml::xml_document<char> dom;
        auto root = xml::ParseXmlFromStream(ifs, buffer, dom, "einstellungen");

        ASSERT_TRUE(root != nullptr);
        EXPECT_TRUE(GetXmlNameSubstring(root) == "einstellungen");
    }

    /// <summary>
    /// Tests parsing XML content from a file.
    /// </summary>
    TEST(Framework_XmlParser_TestCase, Parse_File_Test)
    {
#ifndef _3FD_PLATFORM_WINRT
        const std::string filePath("_dummy.xml");
#else
        const std::string filePath = utils::WinRTExt::GetFilePathUtf8(
            "_dummy.xml",
            utils::WinRTExt::FileLocation::LocalFolder
        );
#endif
        std::ofstream ofs;
        ofs.open(filePath, std::ios_base::trunc);
        ASSERT_TRUE(ofs.is_open());
        ofs.write(xmlContent, strlen(xmlContent));
        ASSERT_FALSE(ofs.bad());
        ofs.close();

        std::vector<char> buffer;
        rapidxml::xml_document<char> dom;
        auto root = xml::ParseXmlFromFile(filePath, buffer, dom, "einstellungen");

        ASSERT_TRUE(root != nullptr);
        EXPECT_TRUE(GetXmlNameSubstring(root) == "einstellungen");
    }

    /// <summary>
    /// Fixture for testing the class NameResolver.
    /// </summary>
    class Framework_XmlNameResolver_TestCase : public ::testing::Test
    {
    private:

        const char * const xmlContent2 = R"(
            <?xml version="1.0" encoding="utf-8"?>
            <page xmlns="http://www.xaml.org"
                  xmlns:d="http://www.xaml.org"
                  xmlns:x="http://www.xaml.org/custom"
                  xmlns:ctrl="http://www.xaml.org/controls"
                  xmlns:ms="http://www.microsoft.com">

                <stack orientation="horizontal">
                    <ctrl:label size="10" x:animation="true">erste</ctrl:label>
                    <ctrl:label size="10" x:animation="true">zweite</ctrl:label>
                </stack>
                <d:panel orientation="horizontal">
                    <ctrl:button size="auto" x:animation="true">offen</ctrl:button>
                    <ctrl:button size="auto" x:animation="true">abbrechen</ctrl:button>
                </d:panel>
            </page>
        )";

    protected:

        rapidxml::xml_document<char> m_dom;
        rapidxml::xml_node<char> *m_root;

        std::unique_ptr<xml::NamespaceResolver> m_nsResolver;

    public:

        Framework_XmlNameResolver_TestCase()
            : m_root(nullptr) {}

        /// <summary>
        /// Set up the test fixture.
        /// </summary>
        virtual void SetUp() override
        {
            m_root = xml::ParseXmlFromBuffer(xmlContent2, m_dom, "page");

            ASSERT_TRUE(m_root != nullptr);
            EXPECT_TRUE(GetXmlNameSubstring(m_root) == "page");

            m_nsResolver.reset(new xml::NamespaceResolver());
        }

        void AddAliases();
    };

    struct PairNsAliasUri
    {
        const char *alias;
        const char *uri;
    };

    static constexpr PairNsAliasUri aliasesToUris[] = {
        {"xaml",   "http://www.xaml.org"},
        {"custom", "http://www.xaml.org/custom"},
        {"ctrl",   "http://www.xaml.org/controls"},
        {"cx",     "http://www.xaml.org/controls"},
        {"micro$", "http://www.microsoft.com"}
    };

    void Framework_XmlNameResolver_TestCase::AddAliases()
    {
        for (auto &ns : aliasesToUris)
        {
            m_nsResolver->AddAliasForNsPrefix(ns.alias, ns.uri);
        }
    }

    /// <summary>
    /// Tests loading namespace declarations.
    /// </summary>
    TEST_F(Framework_XmlNameResolver_TestCase, LoadNamespacesPrefixes_Test)
    {
        EXPECT_EQ(5, m_nsResolver->LoadNamespacesFrom(m_root));
        
        for (auto &ns : aliasesToUris)
        {
            EXPECT_TRUE(m_nsResolver->Has(ns.uri)) << "does not have " << ns.uri;
        }
    }

    struct PairNsPrefixUri
    {
        const char *prefix;
        const char *uri;
    };

    static constexpr PairNsPrefixUri prefixesToUris[] = {
        PairNsPrefixUri{""     , "http://www.xaml.org" },
        PairNsPrefixUri{"d:"   , "http://www.xaml.org"},
        PairNsPrefixUri{"x:"   , "http://www.xaml.org/custom"},
        PairNsPrefixUri{"ctrl:", "http://www.xaml.org/controls"},
        PairNsPrefixUri{"ms:"  , "http://www.microsoft.com"}
    };

    /// <summary>
    /// Tests parsing qualified names.
    /// </summary>
    TEST_F(Framework_XmlNameResolver_TestCase, ParseQualifiedName_Test)
    {
        EXPECT_EQ(5, m_nsResolver->LoadNamespacesFrom(m_root));

        for (auto &ns : prefixesToUris)
        {
            std::string nsUri;
            std::string localName;

            std::string qualifiedName(ns.prefix);
            qualifiedName += "etwas";

            EXPECT_TRUE(m_nsResolver->ParseQualifiedName(qualifiedName,
                                                         nsUri,
                                                         localName)) << "cannot parse " << qualifiedName;

            EXPECT_EQ(ns.uri, nsUri);
            EXPECT_EQ("etwas", localName);
        }
    }

    /// <summary>
    /// Tests adding namespace aliases.
    /// </summary>
    TEST_F(Framework_XmlNameResolver_TestCase, AddNamespaceAliases_Test)
    {
        AddAliases();
    }

    struct PairNsAliasPrefix
    {
        xml::xstr qname;
        std::vector<std::string> expectedTranslations;
    };

    // Dumps into a string information regarding a failed test
    static std::string DumpFailedTest(const PairNsAliasPrefix &testPair,
                                      const std::vector<std::string> &outcome)
    {
        std::ostringstream oss;
        oss << "aliased name = " << testPair.qname.to_string() << ",\n exp. translations = { ";

        for (const auto &x : testPair.expectedTranslations)
            oss << x << ", ";

        oss << "},\nbut got = { ";

        for (const auto &x : outcome)
            oss << x << ", ";

        oss << "}";
        return oss.str();
    }

    /// <summary>
    /// Tests translation of alias to prefix.
    /// </summary>
    TEST_F(Framework_XmlNameResolver_TestCase, Alias2PrefixTranslation_Test)
    {
        AddAliases();
        ASSERT_EQ(5, m_nsResolver->LoadNamespacesFrom(m_root));

        const PairNsAliasPrefix tests[] = {
            PairNsAliasPrefix{ xml::xstr("etwas"),        { std::string("etwas") } },
            PairNsAliasPrefix{ xml::xstr("xaml:etwas"),   { std::string("d:etwas"), std::string("etwas") } },
            PairNsAliasPrefix{ xml::xstr("custom:etwas"), { std::string("x:etwas") } },
            PairNsAliasPrefix{ xml::xstr("ctrl:etwas"),   { std::string("ctrl:etwas") } },
            PairNsAliasPrefix{ xml::xstr("cx:etwas"),     { std::string("ctrl:etwas") } },
            PairNsAliasPrefix{ xml::xstr("micro$:etwas"), { std::string("ms:etwas") } },
            PairNsAliasPrefix{ xml::xstr("y:etwas"),      { std::string("y:etwas") } }
        };

        for (const auto &test : tests)
        {
            auto names = m_nsResolver->GetEquivalentNames(test.qname);

            std::sort(names.begin(), names.end());

            EXPECT_TRUE(names.size() == test.expectedTranslations.size()
                        && std::equal(names.begin(),
                                      names.end(),
                                      test.expectedTranslations.begin())) << DumpFailedTest(test, names);
        }
    }

    // Checks whether name of the XML element or attribute is equal to the expected string
    static bool CheckName(const rapidxml::xml_base<char> *obj,
                          const char *expectation)
    {
        return strncmp(expectation, obj->name(), obj->name_size()) == 0
            && strlen(expectation) == obj->name_size();
    }

    /// <summary>
    /// Tests GetFirstChildNodeIn
    /// </summary>
    TEST_F(Framework_XmlNameResolver_TestCase, GetFirstChildNode_Test)
    {
        AddAliases();
        ASSERT_EQ(5, m_nsResolver->LoadNamespacesFrom(m_root));

        auto node = xml::GetFirstChildNodeIn(m_root, xml::xstr("xaml:stack"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr);
        EXPECT_TRUE(CheckName(node, "stack"));

        node = xml::GetFirstChildNodeIn(node, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr);
        EXPECT_TRUE(CheckName(node, "ctrl:label"));

        node = xml::GetFirstChildNodeIn(m_root, xml::xstr("xaml:panel"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr);
        EXPECT_TRUE(CheckName(node, "d:panel"));

        node = xml::GetFirstChildNodeIn(node, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr);
        EXPECT_TRUE(CheckName(node, "ctrl:button"));
    }

    /// <summary>
    /// Tests GetNextSiblingOf on elements.
    /// </summary>
    TEST_F(Framework_XmlNameResolver_TestCase, GetNextSiblingNode_Test)
    {
        AddAliases();
        ASSERT_EQ(5, m_nsResolver->LoadNamespacesFrom(m_root));

        auto node = xml::GetFirstChildNodeIn(m_root, xml::xstr("xaml:stack"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "stack"));

        node = xml::GetFirstChildNodeIn(node, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "ctrl:label"));

        node = xml::GetNextSiblingOf(node, xml::xstr("ctrl:label"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr);
        EXPECT_TRUE(CheckName(node, "ctrl:label"));

        node = xml::GetFirstChildNodeIn(m_root, xml::xstr("xaml:panel"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "d:panel"));

        node = xml::GetFirstChildNodeIn(node, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "ctrl:button"));

        node = xml::GetNextSiblingOf(node, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr);
        EXPECT_TRUE(CheckName(node, "ctrl:button"));
    }

    /// <summary>
    /// Tests GetFirstAttriuteOf
    /// </summary>
    TEST_F(Framework_XmlNameResolver_TestCase, GetFirstAttributeOf_Test)
    {
        AddAliases();
        ASSERT_EQ(5, m_nsResolver->LoadNamespacesFrom(m_root));

        auto node = GetFirstChildNodeIn(m_root, xml::xstr("xaml:stack"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "stack"));

        node = GetFirstChildNodeIn(node, xml::xstr("ctrl:label"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "ctrl:label"));

        auto attribute = GetFirstAttributeOf(node, xml::xstr("xaml:size"), m_nsResolver.get());
        ASSERT_TRUE(attribute != nullptr);
        EXPECT_TRUE(CheckName(attribute, "size"));

        node = GetFirstChildNodeIn(m_root, xml::xstr("xaml:panel"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "d:panel"));

        node = GetFirstChildNodeIn(node, xml::xstr("ctrl:button"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "ctrl:button"));

        attribute = GetFirstAttributeOf(node, xml::xstr("custom:animation"), m_nsResolver.get());
        ASSERT_TRUE(attribute != nullptr);
        EXPECT_TRUE(CheckName(attribute, "x:animation"));
    }

    /// <summary>
    /// Tests GetNextSiblingOf on attributes.
    /// </summary>
    TEST_F(Framework_XmlNameResolver_TestCase, GetNextSiblingAttribute_Test)
    {
        AddAliases();
        ASSERT_EQ(5, m_nsResolver->LoadNamespacesFrom(m_root));

        auto node = GetFirstChildNodeIn(m_root, xml::xstr("xaml:stack"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "stack"));

        node = GetFirstChildNodeIn(node, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "ctrl:label"));

        auto attribute1 = GetFirstAttributeOf(node, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(attribute1 != nullptr);

        auto attribute2 = GetNextSiblingOf(attribute1, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(attribute2 != nullptr);

        EXPECT_TRUE((CheckName(attribute1, "size") && CheckName(attribute2, "x:animation"))
                    || (CheckName(attribute2, "size") && CheckName(attribute1, "x:animation")));

        node = GetFirstChildNodeIn(m_root, xml::xstr("xaml:panel"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "d:panel"));

        node = GetFirstChildNodeIn(node, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr && CheckName(node, "ctrl:button"));

        attribute1 = GetFirstAttributeOf(node, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(attribute1 != nullptr);

        attribute2 = GetNextSiblingOf(attribute1, xml::xstr(nullptr), m_nsResolver.get());
        ASSERT_TRUE(attribute2 != nullptr);

        EXPECT_TRUE((CheckName(attribute1, "size") && CheckName(attribute2, "x:animation"))
                    || (CheckName(attribute2, "size") && CheckName(attribute1, "x:animation")));
    }

    /// <summary>
    /// Tests IsNameEquivalent
    /// </summary>
    TEST_F(Framework_XmlNameResolver_TestCase, IsNameEquivalent_Test)
    {
        AddAliases();
        ASSERT_EQ(5, m_nsResolver->LoadNamespacesFrom(m_root));

        auto node = GetFirstChildNodeIn(m_root, xml::xstr("xaml:stack"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr
                    && xml::IsNameEquivalent(node, xml::xstr("xaml:stack"), m_nsResolver.get()));

        node = GetFirstChildNodeIn(node, xml::xstr("ctrl:label"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr
                    && xml::IsNameEquivalent(node, xml::xstr("ctrl:label"), m_nsResolver.get()));

        auto attribute = GetFirstAttributeOf(node, xml::xstr("xaml:size"), m_nsResolver.get());
        ASSERT_TRUE(attribute != nullptr
                    && xml::IsNameEquivalent(attribute, xml::xstr("xaml:size"), m_nsResolver.get()));

        node = GetFirstChildNodeIn(m_root, xml::xstr("xaml:panel"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr
                    && xml::IsNameEquivalent(node, xml::xstr("xaml:panel"), m_nsResolver.get()));

        node = GetFirstChildNodeIn(node, xml::xstr("ctrl:button"), m_nsResolver.get());
        ASSERT_TRUE(node != nullptr
                    && xml::IsNameEquivalent(node, xml::xstr("ctrl:button"), m_nsResolver.get()));

        attribute = GetFirstAttributeOf(node, xml::xstr("custom:animation"), m_nsResolver.get());
        ASSERT_TRUE(attribute != nullptr
                    && xml::IsNameEquivalent(attribute, xml::xstr("custom:animation"), m_nsResolver.get()));
    }

    /// <summary>
    /// Fixture for testing the XML queries.
    /// </summary>
    class Framework_XmlQuery_TestCase : public ::testing::Test
    {
    protected:

        rapidxml::xml_document<char> m_dom;
        rapidxml::xml_node<char> *m_root;

    public:

        Framework_XmlQuery_TestCase()
            : m_root(nullptr) {}

        /// <summary>
        /// Set up the test fixture.
        /// </summary>
        virtual void SetUp() override
        {
            m_root = xml::ParseXmlFromBuffer(xmlContent, m_dom, "einstellungen");

            ASSERT_TRUE(m_root != nullptr);
            EXPECT_TRUE(GetXmlNameSubstring(m_root) == "einstellungen");
        }
    };

    /// <summary>
    /// Tests query for single (required) element starting from the given node.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_Required_AtRoot_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);

        auto query = xml::QueryElement("einstellungen", xml::Required, {}, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(strncmp("einstellungen", match->name(), match->name_size()) == 0);

        match = nullptr;

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("einstellungen", match->name(), match->name_size()) == 0);

        match = nullptr;

        // will not match:
        query = xml::QueryElement("abwesend", xml::Required);
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for single (optional) element starting from the given node.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_Optional_AtRoot_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);

        auto query = xml::QueryElement("einstellungen", xml::Optional, {}, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(strncmp("einstellungen", match->name(), match->name_size()) == 0);

        match = nullptr;

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("einstellungen", match->name(), match->name_size()) == 0);

        match = nullptr;

        // no match but also no failure, because match is optional:
        query = xml::QueryElement("abwesend", xml::Optional);
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for single (required) element which can be anywhere in the document.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_Required_Anywhere_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);

        auto query = xml::QueryElement("tiere", xml::Required, {}, &match);

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("tiere", match->name(), match->name_size()) == 0);

        match = nullptr;

        // will not match:
        query = xml::QueryElement("abwesend", xml::Required, {}, &match);
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for single (optional) element which can be anywhere in the document.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_Optional_Anywhere_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);

        auto query = xml::QueryElement("tiere", xml::Optional, {}, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("tiere", match->name(), match->name_size()) == 0);

        match = nullptr;

        // no match but also no failure, because match is optional:
        query = xml::QueryElement("abwesend", xml::Optional, {}, &match);
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for single (required) element with a value to match.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_BindingConstraint_Required_Test)
    {
        auto query = xml::QueryElement("sprache", xml::Required, xml::equal_to_copy_of("Deutsch"));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        std::string sprache("Deutsch");
        query = xml::QueryElement("sprache", xml::Required, xml::equal_to_ref_of(sprache));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        // referenced string changed to something else, so no longer a match:
        sprache = "Daenisch";
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        query = xml::QueryElement("version", xml::Required, xml::equal_to_copy_of(6.96), xml::FloatFormat::Fixed);
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        const rapidxml::xml_node<char> *match(nullptr);

        double version(6.96);
        query = xml::QueryElement("version", xml::Required, xml::equal_to_ref_of(version), xml::FloatFormat::Fixed, {}, &match);
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("version", match->name(), match->name_size()) == 0);

        match = nullptr;

        // referenced number changed to something else, so no longer a match:
        version = 10.0;
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for single (optional) element with a value to match.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_BindingConstraint_Optional_Test)
    {
        /* execution of queries that test only the given element (rather than searching the document
         * recursively) will still succeed despite issuing no matches, because match is now optional */

        auto query = xml::QueryElement("sprache", xml::Optional, xml::equal_to_copy_of("Deutsch"));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        std::string sprache("Deutsch");
        query = xml::QueryElement("sprache", xml::Optional, xml::equal_to_ref_of(sprache));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        // referenced string changed to something else, but still succeeds because match is optional:
        sprache = "Daenisch";
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        query = xml::QueryElement("version", xml::Optional, xml::equal_to_copy_of(6.96), xml::FloatFormat::Fixed);
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        const rapidxml::xml_node<char> *match(nullptr);

        double version(6.96);
        query = xml::QueryElement("version", xml::Optional, xml::equal_to_ref_of(version), xml::FloatFormat::Fixed, {}, &match);
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("version", match->name(), match->name_size()) == 0);

        match = nullptr;

        // referenced number changed to something else, but still succeeds because the match is optional:
        version = 10.0;
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for single element with a required value to be parsed.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_BindingParse_Required_Test)
    {
        std::string sprache;
        auto query = xml::QueryElement("sprache", xml::Required, xml::parse_into(sprache));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_EQ("Deutsch", sprache);

        const rapidxml::xml_node<char> *match(nullptr);

        double version;
        query = xml::QueryElement("version", xml::Required, xml::parse_into(version), xml::FloatFormat::Fixed, {}, &match);
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("version", match->name(), match->name_size()) == 0);
        EXPECT_EQ(6.96, version);
    }

    /// <summary>
    /// Tests query for single element with an optional value to be parsed.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_BindingParse_Optional_Test)
    {
        std::string sprache;
        auto query = xml::QueryElement("sprache", xml::Optional, xml::parse_into(sprache));

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(sprache.empty());

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_EQ("Deutsch", sprache);

        const rapidxml::xml_node<char> *match(nullptr);

        double version(0.0);
        query = xml::QueryElement("version", xml::Optional, xml::parse_into(version), xml::FloatFormat::Fixed, {}, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(match == nullptr);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("version", match->name(), match->name_size()) == 0);
        EXPECT_EQ(6.96, version);
    }

    /// <summary>
    /// Tests query for single (required) element with attribute (without binding).
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_WithAttribute_Required_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);
        auto query = xml::QueryElement("lebensmittel", xml::Required, { xml::QueryAttribute("quality") }, &match);
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("lebensmittel", match->name(), match->name_size()) == 0);

        match = nullptr;

        // will not match:
        query = xml::QueryElement("lebensmittel", xml::Required, { xml::QueryAttribute("abwesend") }, &match);
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for single (optional) element with attribute (without binding).
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_WithAttribute_Optional_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);

        auto query = xml::QueryElement("lebensmittel", xml::Optional, { xml::QueryAttribute("quality") }, &match);
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("lebensmittel", match->name(), match->name_size()) == 0);

        match = nullptr;

        // no match but also no failure, because match is optional:
        query = xml::QueryElement("lebensmittel", xml::Optional, { xml::QueryAttribute("abwesend") });
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for single (required) element with attribute (with binding to set constraint for value).
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_WithAttribute_BindingConstraint_Required_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);
        bool quality(true);

        auto query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality", xml::equal_to_copy_of(quality), xml::BooleanFormat::Alpha)
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("lebensmittel", match->name(), match->name_size()) == 0);

        quality = false;

        // still a match, because it has a copy of the past value
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        match = nullptr;

        query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality", xml::equal_to_ref_of(quality), xml::BooleanFormat::Alpha)
            }, &match);

        // not a match, because now the attribute value is different from the contraint
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);

        match = nullptr;

        // again a match:
        quality = true;
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("lebensmittel", match->name(), match->name_size()) == 0);
    }

    /// <summary>
    /// Tests query for single (optional) element with attribute (with binding to set constraint for value).
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_WithAttribute_BindingConstraint_Optional_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);
        bool quality(true);

        auto query =
            xml::QueryElement("lebensmittel", xml::Optional, {
                xml::QueryAttribute("quality", xml::equal_to_copy_of(quality), xml::BooleanFormat::Alpha)
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("lebensmittel", match->name(), match->name_size()) == 0);

        match = nullptr;
        quality = false;

        query =
            xml::QueryElement("lebensmittel", xml::Optional, {
                xml::QueryAttribute("quality", xml::equal_to_ref_of(quality), xml::BooleanFormat::Alpha)
            }, &match);

        // not a match, because now the attribute value is different from
        // the contraint, but still succeeds because the match is optional:
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);

        match = nullptr;

        // again a match:
        quality = true;
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("lebensmittel", match->name(), match->name_size()) == 0);
    }

    /// <summary>
    /// Tests query for single (required) element with attribute (with binding to receive parsed value).
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_WithAttribute_BindingParse_Required_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);
        bool quality(false);

        auto query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality", xml::Required, xml::parse_into(quality), xml::BooleanFormat::Alpha)
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("lebensmittel", match->name(), match->name_size()) == 0);
        EXPECT_EQ(true, quality);

        match = nullptr;

        // wrong format, will not match:
        query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality", xml::Required, xml::parse_into(quality), xml::BooleanFormat::Numeric)
            }, &match);

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for single (optional) element with attribute (with binding to receive parsed value).
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_WithAttribute_BindingParse_Optional_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);
        bool quality(false);

        auto query =
            xml::QueryElement("lebensmittel", xml::Optional, {
                xml::QueryAttribute("quality", xml::Optional, xml::parse_into(quality), xml::BooleanFormat::Alpha)
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("lebensmittel", match->name(), match->name_size()) == 0);
        EXPECT_EQ(true, quality);

        match = nullptr;

        // still succeeds, because the failed attribute match is optional:
        query =
            xml::QueryElement("lebensmittel", xml::Optional, {
                xml::QueryAttribute("quality", xml::Optional, xml::parse_into(quality), xml::BooleanFormat::Numeric) // wrong format
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("lebensmittel", match->name(), match->name_size()) == 0);

        match = nullptr;

        // wrong element, so no match, but still succeeds because the element match is optional:
        query =
            xml::QueryElement("abwesend", xml::Optional, {
                xml::QueryAttribute("quality", xml::Optional, xml::parse_into(quality), xml::BooleanFormat::Alpha)
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);

        match = nullptr;

        // wrong attribute, so no match, but still succeeds because the element match is optional:
        query =
            xml::QueryElement("lebensmittel", xml::Optional, {
                xml::QueryAttribute("abwesend")
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for single element with attribute
    /// (with bindings to both constrain and receive parsed value).
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, SingleElement_WithAttribute_BindingsCombined_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);
        std::string key("Volkswagen");
        int value;

        auto query =
            xml::QueryElement("eintrag", xml::Required, {
                xml::QueryAttribute("schluessel", xml::equal_to_ref_of(key)),
                xml::QueryAttribute("wert", xml::Required, xml::parse_into(value), xml::IntegerFormat::Decimal)
            }, &match);

        /* because all componentes of the query have required matches,
         * the execution can only succeed with a complete match: */

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("eintrag", match->name(), match->name_size()) == 0);
        EXPECT_EQ(17000, value);

        match = nullptr;

        key = "Hyundai";
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("eintrag", match->name(), match->name_size()) == 0);
        EXPECT_EQ(20300, value);

        match = nullptr;

        // will not match:
        key = "abwesend";
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);

        query =
            xml::QueryElement("eintrag", xml::Required, {
                xml::QueryAttribute("schluessel", xml::equal_to_ref_of(key)),
                xml::QueryAttribute("wert", xml::Optional, xml::parse_into(value), xml::IntegerFormat::Hexa)
            }, &match);

        /* this query can still succeed with a partial match as long
         * as the failed attribute match is not required: */

        match = nullptr;

        key = "Volkswagen";
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("eintrag", match->name(), match->name_size()) == 0);

        match = nullptr;

        key = "Hyundai";
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("eintrag", match->name(), match->name_size()) == 0);

        match = nullptr;

        key = "abwesend";
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);

        query =
            xml::QueryElement("eintrag", xml::Optional, {
                xml::QueryAttribute("schluessel", xml::equal_to_ref_of(key)),
                xml::QueryAttribute("wert", xml::Optional, xml::parse_into(value), xml::IntegerFormat::Decimal)
            }, &match);

        /* execution of queries that do not issue a complete match will still succeed, because
         * the match of the top-most element (and hence of the query as a whole) is optional */

        match = nullptr;

        key = "Volkswagen";
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("eintrag", match->name(), match->name_size()) == 0);
        EXPECT_EQ(17000, value);

        match = nullptr;

        key = "Hyundai";
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("eintrag", match->name(), match->name_size()) == 0);
        EXPECT_EQ(20300, value);

        match = nullptr;

        key = "abwesend";
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);

        query =
            xml::QueryElement("eintrag", xml::Optional, {
                xml::QueryAttribute("schluessel", xml::equal_to_ref_of(key)),
                xml::QueryAttribute("wert", xml::Required, xml::parse_into(value), xml::IntegerFormat::Decimal)
            }, &match);

        match = nullptr;

        key = "Volkswagen";
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("eintrag", match->name(), match->name_size()) == 0);
        EXPECT_EQ(17000, value);

        match = nullptr;

        key = "Hyundai";
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("eintrag", match->name(), match->name_size()) == 0);
        EXPECT_EQ(20300, value);

        match = nullptr;

        key = "abwesend";
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);
    }

    /// <summary>
    /// Tests query for a chain of elements starting from the given node.
    /// All matches are required.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_OneBranch_Required_AtRoot_Test)
    {
        const rapidxml::xml_node<char> *match1(nullptr), *match2(nullptr);

        auto query = xml::QueryElement("einstellungen", xml::Required, {
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match2)
        }, &match1);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("einstellungen", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("tiere", match2->name(), match2->name_size()) == 0);

        // will not match:
        query = xml::QueryElement("einstellungen", xml::Required, {
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("abwesend", xml::Required)
            })
        });

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
    }

    /// <summary>
    /// Tests query for a chain of elements starting from the given node.
    /// All matches are optional.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_OneBranch_Optional_AtRoot_Test)
    {
        const rapidxml::xml_node<char> *match1, *match2;

        auto query = xml::QueryElement("einstellungen", xml::Optional, {
            xml::QueryElement("tiere", xml::Optional, {
                xml::QueryElement("eintrag", xml::Optional)
            }, &match2)
        }, &match1);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("einstellungen", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("tiere", match2->name(), match2->name_size()) == 0);

        match1 = match2 = nullptr;

        // also succeeds:
        query = xml::QueryElement("einstellungen", xml::Optional, {
            xml::QueryElement("tiere", xml::Optional, {
                xml::QueryElement("abwesend", xml::Optional)
            }, &match2)
        }, &match1);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("einstellungen", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("tiere", match2->name(), match2->name_size()) == 0);
    }

    /// <summary>
    /// Tests query for a chain of elements starting from the given node.
    /// Matches are a combination of required and optional.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_OneBranch_RequiredPlusOptional_AtRoot_Test)
    {
        const rapidxml::xml_node<char> *match1, *match2;

        auto query = xml::QueryElement("einstellungen", xml::Optional, {
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match2)
        }, &match1);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("einstellungen", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("tiere", match2->name(), match2->name_size()) == 0);

        match1 = match2 = nullptr;

        // succeeds because the top query is optional, but no match is expected:
        query = xml::QueryElement("einstellungen", xml::Optional, {
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("abwesend", xml::Required)
            }, &match2)
        }, &match1);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match1 == nullptr);
        EXPECT_TRUE(match2 == nullptr);

        // also succeds and has matches:
        query = xml::QueryElement("einstellungen", xml::Required, {
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("abwesend", xml::Optional) // no match, but optional
            }, &match2)
        }, &match1);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("einstellungen", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("tiere", match2->name(), match2->name_size()) == 0);
    }

    /// <summary>
    /// Tests query for a chain of elements starting at any position.
    /// All matches are required.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_OneBranch_Required_Anywhere_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);

        auto query =
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match);

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("tiere", match->name(), match->name_size()) == 0);

        // will not match:
        query =
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("abwesend", xml::Required)
            });

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
    }

    /// <summary>
    /// Tests query for a chain of elements starting at any position.
    /// All matches are optional.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_OneBranch_Optional_Anywhere_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);

        auto query =
            xml::QueryElement("tiere", xml::Optional, {
                xml::QueryElement("eintrag", xml::Optional)
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("tiere", match->name(), match->name_size()) == 0);

        match = nullptr;

        query =
            xml::QueryElement("tiere", xml::Optional, {
                xml::QueryElement("abwesend", xml::Optional)
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("tiere", match->name(), match->name_size()) == 0);
    }

    /// <summary>
    /// Tests query for a chain of elements starting at any position.
    /// Matches are a combination of required and optional.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_OneBranch_RequiredAndOptional_Anywhere_Test)
    {
        const rapidxml::xml_node<char> *match(nullptr);

        auto query =
            xml::QueryElement("tiere", xml::Optional, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(match == nullptr);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("tiere", match->name(), match->name_size()) == 0);

        match = nullptr;

        // still succeeds, but no match:
        query =
            xml::QueryElement("tiere", xml::Optional, {
                xml::QueryElement("abwesend", xml::Required)
            }, &match);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(match == nullptr);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(match == nullptr);

        match = nullptr;

        // succeeds:
        query =
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("eintrag", xml::Optional) // no match, but optional
            }, &match);

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(match == nullptr);

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("tiere", match->name(), match->name_size()) == 0);
    }

    /// <summary>
    /// Tests query for a chain of elements with their attributes.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_OneBranch_WithAttribute_Test)
    {
        const rapidxml::xml_node<char> *match1, *match2;

        auto query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality"),
                xml::QueryElement("eintrag", xml::Required, {
                    xml::QueryAttribute("schluessel")
                }, &match2)
            }, &match1);

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("lebensmittel", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("eintrag", match2->name(), match2->name_size()) == 0);

        // will not match:
        query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality"),
                xml::QueryElement("eintrag", xml::Required, {
                    xml::QueryAttribute("abwesend")
                })
            });

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
    }

    /// <summary>
    /// Tests query for a chain of elements with their attributes and bindings to value constraints.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_OneBranch_WithAttribute_BindingConstraint_Test)
    {
        auto query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality", xml::equal_to_copy_of(true), xml::BooleanFormat::Alpha),
                xml::QueryElement("eintrag", xml::Required, {
                    xml::QueryAttribute("schluessel", xml::equal_to_copy_of("Kohlrabi"))
                })
            });

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        // will not match:
        query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality", xml::equal_to_copy_of(false), xml::BooleanFormat::Alpha),
                xml::QueryElement("eintrag", xml::Required, {
                    xml::QueryAttribute("schluessel", xml::equal_to_copy_of("Kohlrabi"))
                })
            });

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
    }

    /// <summary>
    /// Tests query for a chain of elements with their attributes
    /// and bindings to constrain and receive parsed values.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_OneBranch_WithAttribute_BindingsCombined_Test)
    {
        float wert;

        auto query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality", xml::equal_to_copy_of(true), xml::BooleanFormat::Alpha),
                xml::QueryElement("eintrag", xml::Required, {
                    xml::QueryAttribute("schluessel", xml::equal_to_copy_of("Kohlrabi")),
                    xml::QueryAttribute("wert", xml::Required, xml::parse_into(wert))
                })
            });

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_EQ(2.50F, wert);

        // will not match:
        query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality"),
                xml::QueryElement("eintrag", xml::Required, {
                    xml::QueryAttribute("schluessel", xml::equal_to_copy_of(0)),
                    xml::QueryAttribute("wert")
                })
            });

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        const rapidxml::xml_node<char> *match(nullptr);

        // succeeds:
        query =
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryAttribute("quality", xml::equal_to_copy_of(true), xml::BooleanFormat::Alpha),
                xml::QueryElement("eintrag", xml::Required, {
                    xml::QueryAttribute("schluessel", xml::equal_to_copy_of("Kohlrabi")),
                    xml::QueryAttribute("abwesend", xml::Optional, xml::parse_into(wert)) // no match, but optional
                }, &match)
            });

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
        EXPECT_TRUE(strncmp("eintrag", match->name(), match->name_size()) == 0);
    }

    /// <summary>
    /// Tests query for chain of elements.
    /// All matches are required.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_ManyBranches_Required_Test)
    {
        const rapidxml::xml_node<char> *match1(nullptr), *match2(nullptr), *match3(nullptr);

        auto query = xml::QueryElement("einstellungen", xml::Required, {
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match1),
            xml::QueryElement("wagen", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match2),
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match3)
        });

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        EXPECT_TRUE(strncmp("tiere", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("wagen", match2->name(), match2->name_size()) == 0);
        EXPECT_TRUE(strncmp("lebensmittel", match3->name(), match3->name_size()) == 0);

        // will not match:
        query = xml::QueryElement("einstellungen", xml::Required, {
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }),
            xml::QueryElement("wagen", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }),
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryElement("abwesend", xml::Required)
            })
        });

        EXPECT_FALSE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));
    }

    /// <summary>
    /// Tests query for chain of elements.
    /// All matches are optional.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_ManyBranches_Optional_Test)
    {
        const rapidxml::xml_node<char> *match1(nullptr), *match2(nullptr), *match3(nullptr);

        auto query = xml::QueryElement("einstellungen", xml::Optional, {
            xml::QueryElement("tiere", xml::Optional, {
                xml::QueryElement("eintrag", xml::Optional)
            }, &match1),
            xml::QueryElement("wagen", xml::Optional, {
                xml::QueryElement("eintrag", xml::Optional)
            }, &match2),
            xml::QueryElement("lebensmittel", xml::Optional, {
                xml::QueryElement("eintrag", xml::Optional)
            }, &match3)
        });

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        EXPECT_TRUE(strncmp("tiere", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("wagen", match2->name(), match2->name_size()) == 0);
        EXPECT_TRUE(strncmp("lebensmittel", match3->name(), match3->name_size()) == 0);

        match1 = match2 = match3 = nullptr;

        query = xml::QueryElement("einstellungen", xml::Optional, {
            xml::QueryElement("tiere", xml::Optional, {
                xml::QueryElement("eintrag", xml::Optional)
            }, &match1),
            xml::QueryElement("wagen", xml::Optional, {
                xml::QueryElement("eintrag", xml::Optional)
            }, &match2),
            xml::QueryElement("lebensmittel", xml::Optional, {
                xml::QueryElement("abwesend", xml::Optional)
            }, &match3)
        });

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        EXPECT_TRUE(strncmp("tiere", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("wagen", match2->name(), match2->name_size()) == 0);
        EXPECT_TRUE(strncmp("lebensmittel", match3->name(), match3->name_size()) == 0);
    }

    /// <summary>
    /// Tests query for chain of elements.
    /// The matches are a combination of required and optional.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_ManyBranches_RequiredAndOptional_Test)
    {
        const rapidxml::xml_node<char> *match1(nullptr), *match2(nullptr), *match3(nullptr);

        auto query = xml::QueryElement("einstellungen", xml::Optional, {
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match1),
            xml::QueryElement("wagen", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match2),
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match3)
        });

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        EXPECT_TRUE(strncmp("tiere", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("wagen", match2->name(), match2->name_size()) == 0);
        EXPECT_TRUE(strncmp("lebensmittel", match3->name(), match3->name_size()) == 0);

        match1 = match2 = match3 = nullptr;

        query = xml::QueryElement("einstellungen", xml::Required, {
            xml::QueryElement("tiere", xml::Required, {
                xml::QueryElement("eintrag", xml::Optional)
            }, &match1),
            xml::QueryElement("wagen", xml::Required, {
                xml::QueryElement("eintrag", xml::Optional)
            }, &match2),
            xml::QueryElement("lebensmittel", xml::Required, {
                xml::QueryElement("abwesend", xml::Optional)
            }, &match3)
        });

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        EXPECT_TRUE(strncmp("tiere", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("wagen", match2->name(), match2->name_size()) == 0);
        EXPECT_TRUE(strncmp("lebensmittel", match3->name(), match3->name_size()) == 0);

        match1 = match2 = match3 = nullptr;

        query = xml::QueryElement("einstellungen", xml::Required, {
            xml::QueryElement("tiere", xml::Optional, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match1),
            xml::QueryElement("wagen", xml::Optional, {
                xml::QueryElement("eintrag", xml::Required)
            }, &match2),
            xml::QueryElement("lebensmittel", xml::Optional, {
                xml::QueryElement("abwesend", xml::Required)
            }, &match3)
        });

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        EXPECT_TRUE(strncmp("tiere", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("wagen", match2->name(), match2->name_size()) == 0);

        EXPECT_TRUE(match3 == nullptr);
    }

    /// <summary>
    /// Tests query for chain of elements with their attributes
    /// and bindings to constrain and receive the parsed values.
    /// </summary>
    TEST_F(Framework_XmlQuery_TestCase, ElementChain_ManyBranches_WithAttributes_BindingsCombined_Test)
    {
        const rapidxml::xml_node<char> *match1(nullptr),
                                       *match2(nullptr),
                                       *match3(nullptr),
                                       *match4(nullptr);
        float floatValue;
        int intValue;
        std::string strValue;

        auto query = xml::QueryElement("einstellungen", xml::Required, {
            xml::QueryElement("tiere", xml::Optional, {
                xml::QueryElement("eintrag", xml::Optional, {
                    xml::QueryAttribute("schluessel", xml::equal_to_copy_of("Hund")),
                    xml::QueryAttribute("wert", xml::Required, xml::parse_into(strValue))
                })
            }, &match1),
            xml::QueryElement("wagen", xml::Optional, {
                xml::QueryElement("eintrag", xml::Optional, {
                    xml::QueryAttribute("schluessel", xml::equal_to_copy_of("Hyundai")),
                    xml::QueryAttribute("wert", xml::Required, xml::parse_into(intValue))
                })
            }, &match2),
            xml::QueryElement("lebensmittel", xml::Optional, {
                xml::QueryElement("eintrag", xml::Optional, {
                    xml::QueryAttribute("schluessel", xml::equal_to_copy_of("Champignon")),
                    xml::QueryAttribute("wert", xml::Required, xml::parse_into(floatValue))
                }),
                xml::QueryElement("eintrag", xml::Optional, {
                    xml::QueryAttribute("schluessel", xml::equal_to_copy_of("abwesend"))
                })
            }, &match3),
            xml::QueryElement("abwesend", xml::Optional, {}, &match4)
        });

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively));

        EXPECT_TRUE(strncmp("tiere", match1->name(), match1->name_size()) == 0);
        EXPECT_TRUE(strncmp("wagen", match2->name(), match2->name_size()) == 0);
        EXPECT_TRUE(strncmp("lebensmittel", match3->name(), match3->name_size()) == 0);

        EXPECT_TRUE(match4 == nullptr);

        EXPECT_EQ("Au Au", strValue);
        EXPECT_EQ(20300, intValue);
        EXPECT_EQ(3.50F, floatValue);
    }

    /// <summary>
    /// Fixture for testing the XML queries with namespaces.
    /// </summary>
    class Framework_XmlQueryNS_TestCase : public ::testing::Test
    {
    private:

        const char * const xmlContent = R"(
            <?xml version="1.0" encoding="utf-8"?>
            <einstellungen xmlns="http://3fd.de"
                           xmlns:a="http://3fd.de/a"
                           xmlns:b="http://3fd.de/b"
                           xmlns:c="http://3fd.de/c">
                <sprache>Deutsch</sprache>
                <version>6.96</version>
                <a:tiere>
                    <b:eintrag c:schluessel="Hund" wert="Au Au" />
                    <b:eintrag c:schluessel="Katze" wert="Miau" />
                </a:tiere>
                <a:wagen>
                    <b:eintrag c:schluessel="Volkswagen" wert="17000" />
                    <b:eintrag c:schluessel="Hyundai" wert="20300" />
                </a:wagen>
                <a:lebensmittel quality="true">
                    <b:eintrag c:schluessel="Champignon" wert="3.50" />
                    <b:eintrag c:schluessel="Kohlrabi" wert="2.50" />
                </a:lebensmittel>
            </einstellungen>
        )";

    protected:

        rapidxml::xml_document<char> m_dom;
        rapidxml::xml_node<char> *m_root;

        std::unique_ptr<xml::NamespaceResolver> m_nsResolver;

    public:

        Framework_XmlQueryNS_TestCase()
            : m_root(nullptr) {}

        /// <summary>
        /// Set up the test fixture.
        /// </summary>
        virtual void SetUp() override
        {
            m_root = xml::ParseXmlFromBuffer(xmlContent, m_dom, "einstellungen");

            ASSERT_TRUE(m_root != nullptr);
            EXPECT_TRUE(GetXmlNameSubstring(m_root) == "einstellungen");

            m_nsResolver.reset(new xml::NamespaceResolver());
        }

        void AddAliases()
        {
            m_nsResolver->AddAliasForNsPrefix("s", "http://3fd.de");
            m_nsResolver->AddAliasForNsPrefix("alpha", "http://3fd.de/a");
            m_nsResolver->AddAliasForNsPrefix("beta", "http://3fd.de/b");
            m_nsResolver->AddAliasForNsPrefix("charlie", "http://3fd.de/c");
        }
    };

    /// <summary>
    /// Tests query for chain of elements with their attributes
    /// and bindings to constrain and receive the parsed values.
    /// </summary>
    TEST_F(Framework_XmlQueryNS_TestCase, ElementChain_ManyBranches_WithAttributes_BindingsCombined_Test)
    {
        AddAliases();
        ASSERT_EQ(4, m_nsResolver->LoadNamespacesFrom(m_root));

        const rapidxml::xml_node<char> *match1(nullptr),
                                       *match2(nullptr),
                                       *match3(nullptr),
                                       *match4(nullptr);
        float floatValue;
        int intValue;
        std::string strValue;

        auto query = xml::QueryElement("s:einstellungen", xml::Required, {
            xml::QueryElement("alpha:tiere", xml::Optional, {
                xml::QueryElement("beta:eintrag", xml::Optional, {
                    xml::QueryAttribute("charlie:schluessel", xml::equal_to_copy_of("Hund")),
                    xml::QueryAttribute("wert", xml::Required, xml::parse_into(strValue))
                })
            }, &match1),
            xml::QueryElement("alpha:wagen", xml::Optional, {
                xml::QueryElement("beta:eintrag", xml::Optional, {
                    xml::QueryAttribute("charlie:schluessel", xml::equal_to_copy_of("Hyundai")),
                    xml::QueryAttribute("wert", xml::Required, xml::parse_into(intValue))
                })
            }, &match2),
            xml::QueryElement("alpha:lebensmittel", xml::Optional, {
                xml::QueryElement("beta:eintrag", xml::Optional, {
                    xml::QueryAttribute("charlie:schluessel", xml::equal_to_copy_of("Champignon")),
                    xml::QueryAttribute("wert", xml::Required, xml::parse_into(floatValue))
                }),
                xml::QueryElement("beta:eintrag", xml::Optional, {
                    xml::QueryAttribute("charlie:schluessel", xml::equal_to_copy_of("abwesend"))
                })
            }, &match3),
            xml::QueryElement("abwesend", xml::Optional, {}, &match4)
        });

        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement, m_nsResolver.get()));
        EXPECT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsAllDescendantsRecursively, m_nsResolver.get()));

        EXPECT_TRUE(CheckName(match1, "a:tiere"));
        EXPECT_TRUE(CheckName(match2, "a:wagen"));
        EXPECT_TRUE(CheckName(match3, "a:lebensmittel"));

        EXPECT_TRUE(match4 == nullptr);

        EXPECT_EQ("Au Au", strValue);
        EXPECT_EQ(20300, intValue);
        EXPECT_EQ(3.50F, floatValue);
    }

    /// <summary>
    /// Another fixture for testing the XML queries with namespaces.
    /// </summary>
    class Framework_XmlQueryNS2_TestCase : public ::testing::Test
    {
    private:

        const char * const xmlContent = R"(
            <?xml version="1.0" encoding="utf-8"?>
            <wsdl:definitions
                xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
                xmlns:wsp="http://schemas.xmlsoap.org/ws/2004/09/policy"
                xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd"
                xmlns:wsaw="http://www.w3.org/2006/05/addressing/wsdl"
                xmlns:soap="http://schemas.xmlsoap.org/wsdl/soap12/"
                xmlns:xsd="http://www.w3.org/2001/XMLSchema"
                xmlns:tns="http://calculator.example.org/"
                xmlns:binp="http://schemas.microsoft.com/ws/06/2004/mspolicy/netbinary1"
                xmlns:httpp="http://schemas.microsoft.com/ws/06/2004/policy/http"
                targetNamespace="http://calculator.example.org/">

                <!-- The service endpoints: -->
                <wsdl:service name="CalculatorService">
                    <wsdl:port name="CalculatorEndpointHeaderAuthSSL" binding="tns:CalcBindingHeaderAuthSSL">
                        <soap:address location="https://hostname:8888/calculator"/>
                    </wsdl:port>
                    <wsdl:port name="CalculatorEndpointSSL" binding="tns:CalcBindingSSL">
                        <soap:address location="https://hostname:8989/calculator"/>
                    </wsdl:port>
                    <wsdl:port name="CalculatorEndpointUnsecure" binding="tns:CalcBindingUnsecure">
                        <soap:address location="http://hostname:81/calculator"/>
                    </wsdl:port>
                </wsdl:service>

            </wsdl:definitions>
        )";

    protected:

        rapidxml::xml_document<char> m_dom;
        rapidxml::xml_node<char> *m_root;

        std::unique_ptr<xml::NamespaceResolver> m_nsResolver;

    public:

        Framework_XmlQueryNS2_TestCase()
            : m_root(nullptr) {}

        /// <summary>
        /// Set up the test fixture.
        /// </summary>
        virtual void SetUp() override
        {
            m_root = xml::ParseXmlFromBuffer(xmlContent, m_dom, "wsdl:definitions");

            ASSERT_TRUE(m_root != nullptr);
            EXPECT_TRUE(GetXmlNameSubstring(m_root) == "wsdl:definitions");

            m_nsResolver.reset(new xml::NamespaceResolver());
            m_nsResolver->AddAliasForNsPrefix("wsdl", "http://schemas.xmlsoap.org/wsdl/");
        }
    };

    /// <summary>
    /// Tests query for real application using WSDL.
    /// </summary>
    TEST_F(Framework_XmlQueryNS2_TestCase, WSDL_GrabServicePort_Test)
    {
        ASSERT_EQ(9, m_nsResolver->LoadNamespacesFrom(m_root));

        const rapidxml::xml_node<char> *elementPort(nullptr);

        std::string targetNamespace;
        std::string serviceName;

        auto query = xml::QueryElement("wsdl:definitions", xml::Required, {
                xml::QueryAttribute("targetNamespace", xml::Required, xml::parse_into(targetNamespace)),
                xml::QueryElement("wsdl:service", xml::Required, {
                    xml::QueryAttribute("name", xml::Required, xml::parse_into(serviceName)),
                    xml::QueryElement("wsdl:port", xml::Required, {}, &elementPort)
                })
            });

        ASSERT_TRUE(query->Execute(m_root, xml::QueryStrategy::TestsOnlyGivenElement, m_nsResolver.get()));
        ASSERT_TRUE(elementPort != nullptr);
        EXPECT_TRUE(CheckName(elementPort, "wsdl:port"));
        EXPECT_EQ("http://calculator.example.org/", targetNamespace);
        EXPECT_EQ("CalculatorService", serviceName);
    }

}// end of namespace unit_tests
}// end of namespace _3fd
