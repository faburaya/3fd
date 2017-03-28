#include "stdafx.h"
#include "MetadataCopier.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include <MsXml6.h>
#include <map>
#include <set>
#include <array>
#include <sstream>
#include <codecvt>
#include <algorithm>

/* XML parsing validates the content against the referenced schema (XSD), thus the calls for
   browsing the DOM are not supposed to fail. Their results are only checked because failures
   such as running out of memory can always happen. */

#define CHECK(COM_CALL_EXPR) if ((hr = (COM_CALL_EXPR)) < 0) { WWAPI::RaiseHResultException(hr, "Unexpected error in COM interface call", #COM_CALL_EXPR); }

namespace application
{
    using namespace _3fd::core;

    /// <summary>
    /// Extracts the BSTR from a COM wrapped VARIANT, bypassing its deallocation.
    /// </summary>
    /// <param name="wrappedVar">The wrapped VARIANT.</param>
    /// <returns>The extracted BSTR;</returns>
    static BSTR ExtractBstrFrom(CComVariant &wrappedVar)
    {
        _ASSERTE(wrappedVar.vt == VT_BSTR);
        VARIANT rawVar;
        VariantInit(&rawVar);
        wrappedVar.Detach(&rawVar);
        return rawVar.bstrVal;
    }

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
            CHECK(attrNode->get_nodeValue(&formatNameAsVariant));
            _ASSERTE(formatNameAsVariant.vt == VT_BSTR);

            auto iter = containerFormatByName.find(
                HashName(formatNameAsVariant.bstrVal)
            );

            // does the format have a GUID?
            if (containerFormatByName.end() == iter)
            {
                CComBSTR xmlSource;
                elemNode->get_xml(&xmlSource);

                std::wostringstream woss;
                woss << L"Invalid setting in configuration file! Microsoft WIC GUID was not defined for container format "
                     << formatNameAsVariant.bstrVal << L". Ocurred in:\r\n" << xmlSource;

                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
            }

            hashedFormatNames[idx] = iter->second;
        }

        return MakeKey(hashedFormatNames[0], hashedFormatNames[1]);
    }

    /// <summary>
    /// Represents an entry in the list of metadata map cases.
    /// </summary>
    struct MapCaseEntry
    {
        uint64_t key;
        BSTR fromPath;
        BSTR toPath;
        uint32_t metaFmtNameHash;
        bool onlyCommon;
    };

    typedef std::vector<MapCaseEntry> VecOfCaseEntries;

    /// <summary>
    /// Binary search in sub-range of vector containing map cases entries.
    /// </summary>
    /// <param name="searchKey">The key to search for.</param>
    /// <param name="begin">An iterator to the first position of the sub-range.
    /// In the end, this parameter keeps the beginning of the last sub-range this
    /// iterative algorithm has delved into.</param>
    /// <param name="begin">An iterator to one past the last position of the sub-range.
    /// In the end, this parameter keeps the end of the last sub-range this
    /// iterative algorithm has delved into.</param>
    /// <returns>An iterator to the found entry. Naturally, it cannot refer to
    /// the same position in the parameter 'end', unless there was no match.</returns>
    VecOfCaseEntries::const_iterator BinarySearch(uint64_t searchKey,
                                                  VecOfCaseEntries::const_iterator &begin,
                                                  VecOfCaseEntries::const_iterator &end)
    {
        auto endOfRange = end;

        while (begin != end)
        {
            auto middle = begin + std::distance(begin, end) / 2;

            if (middle->key < searchKey)
                begin = middle + 1;
            else if (middle->key > searchKey)
                end = middle;
            else
                return middle;
        }

        return endOfRange;
    }

    /// <summary>
    /// Holds the metadata map cases loaded from configuration file.
    /// </summary>
    class MetadataMapCases
    {
    private:

        VecOfCaseEntries m_mapCasesEntries;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="MetadataMapCases" /> class.
        /// </summary>
        /// <param name="dom">The DOM parsed from the XML configuration file.</param>
        /// <param name="mapCasesXPath">The XPath to the map cases.</param>
        /// <param name="containerFormatByName">A dictionary of container formats ordered by the hash of their names.</param>
        /// <param name="metadataFormatByName">A dictionary of metadata formats ordered by the hash of their names.</param>
        MetadataMapCases(IXMLDOMDocument2 *dom,
                         BSTR mapCasesXPath,
                         const Hash2HashMap &containerFormatByName,
                         const Hash2HashMap &metadataFormatByName)
        {
            CALL_STACK_TRACE;

            try
            {
                HRESULT hr;
                std::set<decltype(MapCaseEntry::key)> uniqueKeys;

                const CComBSTR attrNameMetaFormat(L"metaFormat"),
                               attrNamePathFrom(L"fromPath"),
                               attrNamePathTo(L"toPath"),
                               attrNameOnlyCommon(L"onlyCommon");

                CComPtr<IXMLDOMNodeList> listOfMapCaseNodes;
                CHECK(dom->selectNodes(mapCasesXPath, &listOfMapCaseNodes));

                long mapCaseNodesCount;
                CHECK(listOfMapCaseNodes->get_length(&mapCaseNodesCount));

                std::vector<MapCaseEntry> mapCasesEntries;
                mapCasesEntries.reserve(mapCaseNodesCount);

                // Iterate over list of map cases:

                for (long idxCase = 0; idxCase < mapCaseNodesCount; ++idxCase)
                {
                    CComPtr<IXMLDOMNode> mapCaseNode;
                    CHECK(listOfMapCaseNodes->get_item(idxCase, &mapCaseNode));

                    auto mapCaseKey = GetMetadataMapCaseKey(mapCaseNode, containerFormatByName);
                    
                    // is map case unique?
                    if (!uniqueKeys.insert(mapCaseKey).second)
                    {
                        std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                        std::wostringstream woss;
                        CComBSTR xmlSource;
                        mapCaseNode->get_xml(&xmlSource);
                        woss << L"Configuration file cannot have duplicated metadata map cases! Ocurred in:\r\n" << xmlSource;
                        throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                    }

                    // Iterate over list entries:

                    long entryNodesCount;
                    CComPtr<IXMLDOMNodeList> listOfEntryNodes;
                    CHECK(mapCaseNode->get_childNodes(&listOfEntryNodes));
                    CHECK(listOfEntryNodes->get_length(&entryNodesCount));

                    for (long idxEntry = 0; idxEntry < entryNodesCount; ++idxEntry)
                    {
                        mapCasesEntries.emplace_back();
                        auto &newEntry = mapCasesEntries.back();
                        newEntry.key = mapCaseKey;

                        CComPtr<IXMLDOMNode> entryNode;
                        CHECK(listOfEntryNodes->get_item(idxEntry, &entryNode));

                        CComPtr<IXMLDOMNamedNodeMap> attributes;
                        CHECK(entryNode->get_attributes(&attributes));

                        // get metadata format:

                        CComVariant metaFormatNameAsVar;
                        CComPtr<IXMLDOMNode> metaFormatAttrNode;
                        CHECK(attributes->getNamedItem(attrNameMetaFormat, &metaFormatAttrNode));
                        CHECK(metaFormatAttrNode->get_nodeValue(&metaFormatNameAsVar));
                        newEntry.metaFmtNameHash = HashName(metaFormatNameAsVar.bstrVal);

                        // does the format have a GUID?
                        if (metadataFormatByName.find(newEntry.metaFmtNameHash) == metadataFormatByName.end())
                        {
                            CComBSTR xmlSource;
                            mapCaseNode->get_xml(&xmlSource);

                            std::wostringstream woss;
                            woss << L"Invalid setting in configuration file! Microsoft WIC GUID was not defined for metadata format "
                                 << metaFormatNameAsVar.bstrVal << L". Ocurred in:\r\n" << xmlSource;

                            std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                            throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                        }

                        // get metadata source path:

                        CComVariant fromPathAsVar;
                        CComPtr<IXMLDOMNode> fromPathAttrNode;
                        CHECK(attributes->getNamedItem(attrNamePathFrom, &fromPathAttrNode));
                        CHECK(fromPathAttrNode->get_nodeValue(&fromPathAsVar));
                        newEntry.fromPath = ExtractBstrFrom(fromPathAsVar);

                        // get metadata destination path:

                        CComVariant toPathAsVar;
                        CComPtr<IXMLDOMNode> toPathAttrNode;
                        CHECK(attributes->getNamedItem(attrNamePathTo, &toPathAttrNode));
                        CHECK(toPathAttrNode->get_nodeValue(&toPathAsVar));
                        newEntry.toPath = ExtractBstrFrom(toPathAsVar);

                        // copy only common items?

                        CComVariant onlyCommonAsVar;
                        CComPtr<IXMLDOMNode> onlyCommonAttrNode;
                        CHECK(attributes->getNamedItem(attrNameOnlyCommon, &onlyCommonAttrNode));
                        CHECK(onlyCommonAttrNode->get_nodeValue(&onlyCommonAsVar));
                        _ASSERTE(onlyCommonAsVar.vt == VT_BOOL);
                        newEntry.onlyCommon = (onlyCommonAsVar.boolVal == VARIANT_TRUE);

                    }// end of inner for loop

                }// end of outer for loop

                // sort the entries by key:
                std::sort(mapCasesEntries.begin(), mapCasesEntries.end(),
                    [](const MapCaseEntry &left, const MapCaseEntry &right) { return left.key < right.key; }
                );

                m_mapCasesEntries.swap(mapCasesEntries);
            }
            catch (IAppException &)
            {
                throw; // just forward exceptions for errors known to have been already handled
            }
            catch (CAtlException &ex)
            {
                WWAPI::RaiseHResultException(ex, "Failed to read metadata map cases from configuration", "COM ATL");
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when reading metadata map cases from configuration: " << ex.what();
                throw AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="MetadataMapCases"/> class.
        /// </summary>
        ~MetadataMapCases()
        {

        }

        /// <summary>
        /// Gets the sub range of entries that match the given key.
        /// </summary>
        /// <param name="searchKey">The key to search.</param>
        /// <param name="subRangeBegin">An iterator to the first position of the sub-range.</param>
        /// <param name="subRangeEnd">An iterator to one past the last position of the sub-range.</param>
        /// <returns>When a sub-range has been found, <c>true</c>, otherwise, <c>false</c>.</returns>
        bool GetSubRange(uint64_t searchKey,
                         VecOfCaseEntries::const_iterator &subRangeBegin,
                         VecOfCaseEntries::const_iterator &subRangeEnd) const
        {
            struct {
                VecOfCaseEntries::const_iterator begin;
                VecOfCaseEntries::const_iterator end;
            } firstMatchRightPart;

            auto begin = m_mapCasesEntries.begin();
            auto end = m_mapCasesEntries.end();
            auto match = BinarySearch(searchKey, begin, end);

            // match? this is the first, so keep the partition at the right:
            if (match != end)
            {
                firstMatchRightPart.begin = match; // match inclusive
                firstMatchRightPart.end = end;
            }
            else
            {
                subRangeBegin = subRangeEnd = m_mapCasesEntries.end();
                return false; // no match found!
            }

            do
            {
                subRangeBegin = end = match; // last match is the smallest entry found!
                match = BinarySearch(searchKey, begin, end); // continue to search in the left partition
            }
            while (match != end);

            /* Now go back to the partition at the right of the first
               match, and start looking for the end of the range: */

            match = firstMatchRightPart.begin;
            end = firstMatchRightPart.end;

            do
            {
                subRangeEnd = begin = match + 1; // last match is the greatest entry found!
                match = BinarySearch(searchKey, begin, end); // continue to search in the right partition
            }
            while (match != end);

            return true;
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
                CComPtr<IXMLDOMNode> guidAttrNode;
                CHECK(attributes->getNamedItem(attrNameGuid, &guidAttrNode));
                CHECK(guidAttrNode->get_nodeValue(&guidAsVariant));
                _ASSERTE(guidAsVariant.vt == VT_BSTR);
                CHECK(IIDFromString(guidAsVariant.bstrVal, &guid));
                uint32_t hashedGuid = HashGuid(guid);

                // get name:

                CComVariant nameAsVariant;
                CComPtr<IXMLDOMNode> nameAttrNode;
                CHECK(attributes->getNamedItem(attrNameName, &nameAttrNode));
                CHECK(nameAttrNode->get_nodeValue(&nameAsVariant));
                _ASSERTE(nameAsVariant.vt == VT_BSTR);
                uint32_t hashedName = HashName(nameAsVariant.bstrVal);

                // check whether GUID is unique in list:
                if (!nameByGuid.emplace(hashedGuid, hashedName).second)
                {
                    CComBSTR xmlSource;
                    node->get_xml(&xmlSource);

                    std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                    std::wostringstream woss;
                    woss << L"Configuration file cannot have duplicated GUID in list of formats: ocurred in " << xmlSource;
                    throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                }

                // check whether name is unique in list:
                if (!guidByName.emplace(hashedName, hashedGuid).second)
                {
                    CComBSTR xmlSource;
                    node->get_xml(&xmlSource);
                    
                    std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                    std::wostringstream woss;
                    woss << L"Configuration file cannot have duplicated name in list of formats: ocurred in " << xmlSource;
                    throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                }
            }
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions for errors known to have been already handled
        }
        catch (CAtlException &ex)
        {
            WWAPI::RaiseHResultException(ex, "Failed to read formats from configuration", "COM ATL");
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
        catch (CAtlException &ex)
        {
            WWAPI::RaiseHResultException(ex, "Failed when loading configuration", "COM ATL");
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when loading configuration: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

}// end of namespace application
