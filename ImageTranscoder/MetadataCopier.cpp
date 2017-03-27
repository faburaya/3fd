#include "stdafx.h"
#include "MetadataCopier.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include <MsXml6.h>
#include <map>
#include <array>
#include <sstream>
#include <codecvt>


/* XML parsing validates the content against the referenced schema (XSD), thus the calls for
   browsing the DOM are not supposed to fail. Their results are only checked because failures
   such as running out of memory can always happen. */

#define CHECK(COM_CALL_EXPR) if ((hr = (COM_CALL_EXPR)) < 0) { WWAPI::RaiseHResultException(hr, "Unexpected error in COM interface call", #COM_CALL_EXPR); }

namespace application
{
    using namespace _3fd::core;

    /// <summary>
    /// Hashes the unique identifier using FNV1a algorithm.
    /// </summary>
    /// <param name="guid">The unique identifier.</param>
    /// <returns>The GUID hashed to an unsigned 32 bits integer.</returns>
    static uint32_t HashGuid(const GUID &guid)
    {
        uint32_t hash(2166136261);

        auto data = (uint8_t *)&guid;
        for (int idx = 0; idx < sizeof guid; ++idx)
        {
            hash = hash ^ data[idx];
            hash = static_cast<uint32_t> (hash * 16777619ULL);
        }

        return hash;
    }

    /// <summary>
    /// Makes the key by concatenating 2 hash values.
    /// </summary>
    /// <param name="low">The low part of the key.</param>
    /// <param name="high">The high part of the key.</param>
    /// <returns>The key as a 64 bits long unsigned integer.</returns>
    static uint64_t MakeKey(uint32_t low, uint32_t high)
    {
        struct { uint32_t low, high; } key;
        key.low = low;
        key.high = high;
        return *reinterpret_cast<uint64_t *> (&key);
    }

    /// <summary>
    /// Makes a key given the source and destination format GUID's.
    /// </summary>
    /// <param name="srcFormatGuid">The source format unique identifier.</param>
    /// <param name="destFormatGuid">The destination format unique identifier.</param>
    /// <returns>The key as a 64 bits long unsigned integer.</returns>
    static uint64_t MakeKey(const GUID &srcFormatGuid, const GUID &destFormatGuid)
    {
        return MakeKey(HashGuid(srcFormatGuid), HashGuid(destFormatGuid));
    }

    /// <summary>
    /// Hashes a given BSTR string, using SDBM algorithm.
    /// </summary>
    /// <param name="str">The string, which is a BSTR (UCS-2 encoded),
    /// but supposed to have ASCII characters for better results.</param>
    /// <returns>The key as a 32 bits long unsigned integer.</returns>
    static uint32_t HashName(BSTR str)
    {
        uint32_t hash(0);

        wchar_t ch;
        while (ch = *str++)
            hash = static_cast<uint32_t> (towupper(ch) + (hash << 6) + (hash << 16) - hash);

        return hash;
    }

    typedef std::map<uint32_t, uint32_t> Hash2HashMap;

    
    ////////////////////////////
    // MetadataMapCases Class
    ////////////////////////////

    /// <summary>
    /// Composes the key to search in the dictionary of metadata map cases.
    /// </summary>
    /// <param name="elemNode">The map case node.</param>
    /// <param name="containerFormatByName">A dictionary of container formats ordered by the hash of their names.</param>
    /// <returns>The key composed of the hashed GUID's of both source and destination container formats.</returns>
    static uint64_t GetMetadataMapCaseKey(IXMLDOMNode *elemNode, const Hash2HashMap &containerFormatByName)
    {
        CALL_STACK_TRACE;

        HRESULT hr;
        const CComBSTR attrNameFormats[] = { CComBSTR(L"srcFormat"), CComBSTR(L"destFormat") };

        std::array<uint32_t, sizeof attrNameFormats / sizeof attrNameFormats[0]> hashedFormatNames;

        CComPtr<IXMLDOMNamedNodeMap> attributes;
        CHECK(elemNode->get_attributes(&attributes));

        for (int idx = 0; idx < hashedFormatNames.size(); ++idx)
        {
            // get container format name:
            CComVariant formatNameAsVariant;
            CComPtr<IXMLDOMNode> attrNode;
            CHECK(attributes->getNamedItem(attrNameFormats[idx], &attrNode));
            CHECK(elemNode->get_nodeValue(&formatNameAsVariant));
            _ASSERTE(formatNameAsVariant.vt == VT_BSTR);

            auto iter = containerFormatByName.find(
                HashName(formatNameAsVariant.bstrVal)
            );

            if (containerFormatByName.end() == iter)
            {
                std::wostringstream woss;
                woss << L"Invalid metadata map case in configuration file: "
                        L"Microsoft WIC GUID was not defined for container format " << formatNameAsVariant.bstrVal;

                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
            }

            hashedFormatNames[idx] = iter->second;
        }

        return MakeKey(hashedFormatNames[0], hashedFormatNames[1]);
    }

    /// <summary>
    /// Holds the metadata map cases loaded from configuration file.
    /// </summary>
    class MetadataMapCases
    {
    private:

        struct MapCaseEntry
        {
            uint64_t key;
            BSTR fromPath;
            BSTR toPath;
            uint32_t metaFmtNameHash;
            bool copyAllItems;
        };

        std::vector<MapCaseEntry> m_mapCases;

    public:

        MetadataMapCases(IXMLDOMDocument2 *dom,
                         BSTR mapCasesXPath,
                         const Hash2HashMap &containerFormatByName)
        {
            CALL_STACK_TRACE;

            try
            {
                HRESULT hr;
                CComPtr<IXMLDOMNodeList> listOfMapCaseNodes;
                CHECK(dom->selectNodes(mapCasesXPath, &listOfMapCaseNodes));

                // Iterate over list of map cases:

                long mapCaseNodesCount;
                CHECK(listOfMapCaseNodes->get_length(&mapCaseNodesCount));
                for (long idxCase = 0; idxCase < mapCaseNodesCount; ++idxCase)
                {
                    CComPtr<IXMLDOMNode> mapCaseNode;
                    CHECK(listOfMapCaseNodes->get_item(idxCase, &mapCaseNode));

                    auto mapCaseKey = GetMetadataMapCaseKey(mapCaseNode, containerFormatByName);

                    CComPtr<IXMLDOMNodeList> listOfEntryNodes;
                    CHECK(mapCaseNode->get_childNodes(&listOfEntryNodes));

                    // Iterate over list entries:

                    long entryNodesCount;
                    CHECK(listOfEntryNodes->get_length(&entryNodesCount));
                    for (long idxEntry = 0; idxEntry < entryNodesCount; ++idxEntry)
                    {
                        CComPtr<IXMLDOMNode> entryNode;
                        CHECK(listOfEntryNodes->get_item(idxEntry, &entryNode));

                        const CComBSTR attrNameMetaFormat(L"metaFormat"),
                                       attrNamePathFrom(L"fromPath"),
                                       attrNamePathTo(L"toPath"),
                                       attrNameCopyAllItems(L"copyAllItems");

                        CComPtr<IXMLDOMNamedNodeMap> attributes;
                        CHECK(entryNode->get_attributes(&attributes));

                        // TO DO

                    }// end of inner for loop

                }// end of outer for loop
            }
            catch (IAppException &)
            {
                throw; // just forward exceptions for errors known to have been already handled
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when reading metadata map cases from configuration: " << ex.what();
                throw AppException<std::runtime_error>(oss.str());
            }
        }

    };// end of MetadataMapCases class


    ////////////////////////////
    // MetadataCopier Class
    ////////////////////////////

    /// <summary>
    /// Reads the container and metadata formats in XML configuration file.
    /// </summary>
    /// <param name="dom">The DOM parsed from the XML configuration file.</param>
    /// <param name="listXPath">The XPath to the list of formats.</param>
    /// <param name="nameByGuid">A dictionary of names ordered by unique identifier.</param>
    /// <param name="guidByName">A dictionary of unique identifiers ordered by name.</param>
    static void ReadFormats(IXMLDOMDocument2 *dom,
                            BSTR listXPath,
                            Hash2HashMap &nameByGuid,
                            Hash2HashMap &guidByName)
    {
        CALL_STACK_TRACE;

        try
        {
            HRESULT hr;
            CComPtr<IXMLDOMNodeList> nodeList;
            CHECK(dom->selectNodes(listXPath, &nodeList));

            const CComBSTR attrNameGuid(L"guid"),
                           attrNameName(L"name");

            // Iterate over list of container formats:

            long nodeCount;
            CHECK(nodeList->get_length(&nodeCount));
            for (long idx = 0; idx < nodeCount; ++idx)
            {
                CComPtr<IXMLDOMNode> node;
                CHECK(nodeList->get_item(idx, &node));

                CComPtr<IXMLDOMNamedNodeMap> attributes;
                CHECK(node->get_attributes(&attributes));

                // get GUID:

                GUID guid;
                CComVariant guidAsVariant;
                node.Release();
                CHECK(attributes->getNamedItem(attrNameGuid, &node));
                CHECK(node->get_nodeValue(&guidAsVariant));
                _ASSERTE(guidAsVariant.vt == VT_BSTR);
                CHECK(IIDFromString(guidAsVariant.bstrVal, &guid));
                uint32_t hashedGuid = HashGuid(guid);

                // get name:

                CComVariant nameAsVariant;
                node.Release();
                CHECK(attributes->getNamedItem(attrNameName, &node));
                CHECK(node->get_nodeValue(&nameAsVariant));
                _ASSERTE(nameAsVariant.vt == VT_BSTR);
                uint32_t hashedName = HashName(nameAsVariant.bstrVal);

                // check whether GUID is unique in list:
                if (!nameByGuid.emplace(hashedGuid, hashedName).second)
                {
                    std::wostringstream woss;
                    woss << L"Configuration file cannot have duplicated GUID in list of formats: "
                            L"ocurred in GUID = " << guidAsVariant.bstrVal << L", format = " << nameAsVariant.bstrVal;

                    std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                    throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                }

                // check whether name is unique in list:
                if (!guidByName.emplace(hashedName, hashedGuid).second)
                {
                    std::wostringstream woss;
                    woss << L"Configuration file cannot have duplicated name in list of formats: "
                            L"ocurred in format = " << nameAsVariant.bstrVal << L", GUID = " << guidAsVariant.bstrVal;

                    std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                    throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                }
            }
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions for errors known to have been already handled
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when reading formats from configuration: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="MetadataCopier"/> class.
    /// </summary>
    /// <param name="cfgFilePath">The path for the XML configuration file.</param>
    MetadataCopier::MetadataCopier(const string &cfgFilePath)
    {
        CALL_STACK_TRACE;
        
        try
        {
            // Instantiante the DOM parser:

            CComPtr<IXMLDOMDocument2> dom;
            auto hr = dom.CoCreateInstance(__uuidof(DOMDocument60),
                                           nullptr,
                                           CLSCTX_INPROC_SERVER);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Could not instantiate MSXML6 DOM document parser", "CComPtr<IXMLDOMDocument2>::CoCreateInstance");

            dom->put_async(VARIANT_FALSE);
            dom->put_validateOnParse(VARIANT_TRUE);
            dom->put_resolveExternals(VARIANT_TRUE);

            // Parse the XML document:

            VARIANT_BOOL parserSucceeded;
            hr = dom->load(CComVariant(cfgFilePath.c_str()), &parserSucceeded);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to load XML configuration file into DOM parser", "IXMLDOMDocument2::load");

            if (parserSucceeded == VARIANT_FALSE)
            {
                long lineNumber;
                CComBSTR reason, xmlSource;
                CComPtr<IXMLDOMParseError> parseError;
                dom->get_parseError(&parseError);
                parseError->get_reason(&reason);
                parseError->get_srcText(&xmlSource);
                parseError->get_line(&lineNumber);

                std::wostringstream woss;
                woss << L"Failed to parse configuration file! "
                     << static_cast <LPWSTR> (reason)
                     << L" - at line " << lineNumber << L": "
                     << static_cast<LPWSTR> (xmlSource);

                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
            }

            Hash2HashMap containerFormatByName, containerFormatByGuid;
            ReadFormats(dom, CComBSTR(L"//metadata/formats/container/*"), containerFormatByGuid, containerFormatByName);

            Hash2HashMap metadataFormatByName, metadataFormatByGuid;
            ReadFormats(dom, CComBSTR(L"//metadata/formats/metadata/*"), metadataFormatByGuid, metadataFormatByName);

            m_mapCases.reset(new MetadataMapCases(dom, CComBSTR(L"//metadata/map/*"), containerFormatByName));

            // TO DO
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions for errors known to have been already handled
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when loading configuration: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

}// end of namespace application
