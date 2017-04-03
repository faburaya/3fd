#include "stdafx.h"
#include "MetadataCopier.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\utils_algorithms.h"
#include <MsXml6.h>
#include <map>
#include <set>
#include <array>
#include <memory>
#include <sstream>
#include <codecvt>
#include <algorithm>
#include <cassert>

/* XML parsing validates the content against the referenced schema (XSD), thus the calls for
   browsing the DOM are not supposed to fail. Their results are only checked because failures
   such as running out of memory can always happen. */

#define CHECK(COM_CALL_EXPR) if ((hr = (COM_CALL_EXPR)) < 0) { WWAPI::RaiseHResultException(hr, "Unexpected error in COM interface call", #COM_CALL_EXPR); }

namespace application
{
    using namespace _3fd;
    using namespace _3fd::core;

    using namespace Microsoft::WRL;

    //////////////////
    // Utilities
    //////////////////

    /// <summary>
    /// Extracts the BSTR from a COM wrapped VARIANT, bypassing its deallocation.
    /// </summary>
    /// <param name="wrappedVar">The wrapped VARIANT.</param>
    /// <returns>The extracted BSTR.</returns>
    static BSTR ExtractBstrFrom(_variant_t &wrappedVar)
    {
        _ASSERTE(wrappedVar.vt == VT_BSTR);
        VARIANT rawVar;
        rawVar = wrappedVar.Detach();
        return rawVar.bstrVal;
    }

    /// <summary>
    /// Gets the value reference from a VARIANT.
    /// </summary>
    /// <param name="variant">The variant object.</param>
    /// <returns>A reference to the value held by the VARIANT.</returns>
    template <typename ValType>
    ValType GetValueFrom(_variant_t &variant)
    {
        static_cast(false);
        return variant;
    }

    template <> bool GetValueFrom<bool>(_variant_t &variant) { _ASSERTE(variant.vt == VT_BOOL);  return (variant.boolVal == VARIANT_TRUE); }
    template <> BSTR GetValueFrom<BSTR>(_variant_t &variant) { _ASSERTE(variant.vt == VT_BSTR);  return ExtractBstrFrom(variant); }
    template <> uint16_t GetValueFrom<uint16_t>(_variant_t &variant) { _ASSERTE(variant.vt == VT_UI2);  return variant.uiVal; }
    template <> uint32_t GetValueFrom<uint32_t>(_variant_t &variant) { _ASSERTE(variant.vt == VT_UI4);  return variant.uintVal; }
    template <> int16_t GetValueFrom<int16_t>(_variant_t &variant) { _ASSERTE(variant.vt == VT_I2);  return variant.iVal; }
    template <> int32_t GetValueFrom<int32_t>(_variant_t &variant) { _ASSERTE(variant.vt == VT_I4);  return variant.intVal; }

    /// <summary>
    /// Gets the XML attribute value.
    /// </summary>
    /// <param name="attributes">The attributes.</param>
    /// <param name="attrName">Name of the attribute.</param>
    /// <param name="typeConv">The type conversion to perform.</param>
    /// <param name="value">A reference to the variable to receive the value.</param>
    template <typename ValType>
    void GetAttributeValue(IXMLDOMNamedNodeMap *attributes,
                           BSTR attrName,
                           VARTYPE typeConv,
                           ValType &value)
    {
        CALL_STACK_TRACE;

        HRESULT hr;
        _variant_t variant, variantConv;
        ComPtr<IXMLDOMNode> attrNode;
        CHECK(attributes->getNamedItem(attrName, attrNode.GetAddressOf()));
        CHECK(attrNode->get_nodeValue(variant.GetAddress()));
        
        _ASSERTE(variant.vt == VT_BSTR);
        if (typeConv != VT_BSTR)
        {
            CHECK(VariantChangeType(variantConv.GetAddress(), variant.GetAddress(), 0, typeConv));
            value = GetValueFrom<ValType>(variantConv);
        }
        else
            value = GetValueFrom<ValType>(variant);
    }

    /// <summary>
    /// Gets the XML attribute value as a hash.
    /// </summary>
    /// <param name="attributes">The attributes.</param>
    /// <param name="attrName">Name of the attribute.</param>
    /// <param name="typeConv">The type conversion to perform.</param>
    /// <param name="value">A reference to the variable to receive the value.</param>
    /// <returns>The originally parsed BSTR value.</returns>
    template <typename ValHashType>
    _bstr_t GetAttributeValueHash(IXMLDOMNamedNodeMap *attributes,
                                  BSTR attrName,
                                  ValHashType &hash)
    {
        BSTR value;
        GetAttributeValue(attributes, attrName, VT_BSTR, value);
        hash = HashName(value);
        return _bstr_t(value, false);
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
        const _bstr_t attrNameFormats[] = { _bstr_t(L"srcFormat"), _bstr_t(L"destFormat") };

        std::array<uint32_t, sizeof attrNameFormats / sizeof attrNameFormats[0]> hashedFormatGUIDs;
        std::array<uint32_t, sizeof attrNameFormats / sizeof attrNameFormats[0]> hashedFormatNames;
        
        ComPtr<IXMLDOMNamedNodeMap> attributes;
        CHECK(elemNode->get_attributes(attributes.GetAddressOf()));

        for (int idx = 0; idx < hashedFormatNames.size(); ++idx)
        {
            // get container format name
            auto formatName = GetAttributeValueHash(attributes.Get(), attrNameFormats[idx], hashedFormatNames[idx]);
            
            // does the format have a GUID?
            auto iter = containerFormatByName.find(hashedFormatNames[idx]);
            if (containerFormatByName.end() == iter)
            {
                _bstr_t xmlSource;
                elemNode->get_xml(xmlSource.GetAddress());

                std::wostringstream woss;
                woss << L"Invalid setting in configuration file! Microsoft WIC GUID was not defined for container format "
                     << formatName.GetBSTR() << L". Ocurred in:\r\n" << xmlSource.GetBSTR();

                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
            }

            hashedFormatGUIDs[idx] = iter->second;
        }

        return MakeKey(hashedFormatGUIDs[0], hashedFormatGUIDs[1]);
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

        uint64_t GetKey() const { return key;  }
    };

    typedef std::vector<MapCaseEntry> VecOfCaseEntries;

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

                const _bstr_t attrNameMetaFormat(L"metaFormat"),
                              attrNamePathFrom(L"fromPath"),
                              attrNamePathTo(L"toPath"),
                              attrNameOnlyCommon(L"onlyCommon");

                ComPtr<IXMLDOMNodeList> listOfMapCaseNodes;
                CHECK(dom->selectNodes(mapCasesXPath, listOfMapCaseNodes.GetAddressOf()));

                long mapCaseNodesCount;
                CHECK(listOfMapCaseNodes->get_length(&mapCaseNodesCount));

                std::vector<MapCaseEntry> mapCasesEntries;
                mapCasesEntries.reserve(mapCaseNodesCount);

                // Iterate over list of map cases:

                for (long idxCase = 0; idxCase < mapCaseNodesCount; ++idxCase)
                {
                    ComPtr<IXMLDOMNode> mapCaseNode;
                    CHECK(listOfMapCaseNodes->get_item(idxCase, mapCaseNode.GetAddressOf()));

                    auto mapCaseKey = GetMetadataMapCaseKey(mapCaseNode.Get(), containerFormatByName);
                    
                    // is map case unique?
                    if (!uniqueKeys.insert(mapCaseKey).second)
                    {
                        std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                        std::wostringstream woss;
                        _bstr_t xmlSource;
                        mapCaseNode->get_xml(xmlSource.GetAddress());
                        woss << L"Configuration file cannot have duplicated metadata map cases! Ocurred in:\r\n" << xmlSource.GetBSTR();
                        throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                    }

                    // Iterate over list entries:

                    long entryNodesCount;
                    ComPtr<IXMLDOMNodeList> listOfEntryNodes;
                    CHECK(mapCaseNode->get_childNodes(listOfEntryNodes.GetAddressOf()));
                    CHECK(listOfEntryNodes->get_length(&entryNodesCount));

                    for (long idxEntry = 0; idxEntry < entryNodesCount; ++idxEntry)
                    {
                        mapCasesEntries.emplace_back();
                        auto &newEntry = mapCasesEntries.back();
                        newEntry.key = mapCaseKey;

                        ComPtr<IXMLDOMNode> entryNode;
                        CHECK(listOfEntryNodes->get_item(idxEntry, entryNode.GetAddressOf()));

                        ComPtr<IXMLDOMNamedNodeMap> attributes;
                        CHECK(entryNode->get_attributes(attributes.GetAddressOf()));

                        // get metadata format
                        auto metaFormatName = GetAttributeValueHash(attributes.Get(), attrNameMetaFormat, newEntry.metaFmtNameHash);

                        // does the format have a GUID?
                        if (metadataFormatByName.find(newEntry.metaFmtNameHash) == metadataFormatByName.end())
                        {
                            _bstr_t xmlSource;
                            mapCaseNode->get_xml(xmlSource.GetAddress());

                            std::wostringstream woss;
                            woss << L"Invalid setting in configuration file! Microsoft WIC GUID was not defined for metadata format "
                                 << metaFormatName.GetBSTR() << L". Ocurred in:\r\n" << xmlSource.GetBSTR();

                            std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                            throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                        }

                        GetAttributeValue(attributes.Get(), attrNamePathFrom, VT_BSTR, newEntry.fromPath);
                        GetAttributeValue(attributes.Get(), attrNamePathTo, VT_BSTR, newEntry.toPath);
                        GetAttributeValue(attributes.Get(), attrNameOnlyCommon, VT_BOOL, newEntry.onlyCommon);

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
            catch (_com_error &ex)
            {
                WWAPI::RaiseHResultException(ex, "Failed to read metadata map cases from configuration");
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
            for (auto &entry : m_mapCasesEntries)
            {
                SysFreeString(entry.fromPath);
                SysFreeString(entry.toPath);
            }
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
            return utils::BinSearchSubRange(searchKey, subRangeBegin, subRangeEnd);
        }

    };// end of MetadataMapCases class


    ////////////////////////////
    // MetadataItems Class
    ////////////////////////////

    /// <summary>
    /// Represents an entry in the list of metadata items.
    /// </summary>
    struct ItemEntry
    {
        uint32_t metaFmtNameHash;
        uint16_t id;
        bool rational;
        BSTR name;

        uint32_t GetKey() const { return metaFmtNameHash;  }
    };

    typedef std::vector<ItemEntry> VecOfItems;

    /// <summary>
    /// Holds the metadata items loaded from configuration file.
    /// </summary>
    class MetadataItems
    {
    private:

        VecOfItems m_items;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="MetadataItems" /> class.
        /// </summary>
        /// <param name="dom">The DOM parsed from the XML configuration file.</param>
        /// <param name="itemsXPath">The XPath to the items.</param>
        /// <param name="metadataFormatByName">A dictionary of metadata formats ordered by the hash of their names.</param>
        MetadataItems(IXMLDOMDocument2 *dom,
                      BSTR itemsXPath,
                      const Hash2HashMap &metadataFormatByName)
        {
            CALL_STACK_TRACE;

            try
            {
                HRESULT hr;
                std::set<decltype(ItemEntry::id)> uniqueKeys;

                const _bstr_t attrNameId(L"id"),
                              attrNameMetaFormat(L"metaFormat"),
                              attrNameRational(L"rational"),
                              attrNameName(L"name");

                ComPtr<IXMLDOMNodeList> listOfNodes;
                CHECK(dom->selectNodes(itemsXPath, listOfNodes.GetAddressOf()));

                long nodesCount;
                CHECK(listOfNodes->get_length(&nodesCount));

                std::vector<ItemEntry> items;
                items.reserve(nodesCount);

                // Iterate over list of items:

                for (long idx = 0; idx < nodesCount; ++idx)
                {
                    items.emplace_back();
                    auto &newEntry = items.back();

                    ComPtr<IXMLDOMNode> elemNode;
                    CHECK(listOfNodes->get_item(idx, elemNode.GetAddressOf()));

                    ComPtr<IXMLDOMNamedNodeMap> attributes;
                    CHECK(elemNode->get_attributes(attributes.GetAddressOf()));

                    // get item ID
                    GetAttributeValue(attributes.Get(), attrNameId, VT_UI2, newEntry.id);

                    // is the item ID unique?
                    if (!uniqueKeys.insert(newEntry.id).second)
                    {
                        std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                        std::wostringstream woss;
                        _bstr_t xmlSource;
                        elemNode->get_xml(xmlSource.GetAddress());
                        woss << L"Configuration file cannot have duplicated metadata items! Ocurred in:\r\n" << xmlSource.GetBSTR();
                        throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                    }

                    // get metadata format
                    auto metaFormatName = GetAttributeValueHash(attributes.Get(), attrNameMetaFormat, newEntry.metaFmtNameHash);

                    // does the format have a GUID?
                    if (metadataFormatByName.find(newEntry.metaFmtNameHash) == metadataFormatByName.end())
                    {
                        _bstr_t xmlSource;
                        elemNode->get_xml(xmlSource.GetAddress());

                        std::wostringstream woss;
                        woss << L"Invalid setting in configuration file! Microsoft WIC GUID was not defined for metadata format "
                             << metaFormatName.GetBSTR() << L". Ocurred in:\r\n" << xmlSource.GetBSTR();

                        std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                        throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                    }

                    GetAttributeValue(attributes.Get(), attrNameRational, VT_BOOL, newEntry.rational);
                    GetAttributeValue(attributes.Get(), attrNameName, VT_BSTR, newEntry.name);

                }// end of loop

                // sort the entries by metadata format:
                std::sort(items.begin(), items.end(),
                    [](const ItemEntry &left, const ItemEntry &right) { return left.metaFmtNameHash < right.metaFmtNameHash; }
                );

                m_items.swap(items);
            }
            catch (IAppException &)
            {
                throw; // just forward exceptions for errors known to have been already handled
            }
            catch (_com_error &ex)
            {
                WWAPI::RaiseHResultException(ex, "Failed to read metadata items from configuration");
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when reading metadata items from configuration: " << ex.what();
                throw AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="MetadataItems"/> class.
        /// </summary>
        ~MetadataItems()
        {
            for (auto &entry : m_items)
            {
                SysFreeString(entry.name);
            }
        }

        /// <summary>
        /// Gets the sub range of entries for a given metadata format.
        /// </summary>
        /// <param name="searchKey">The search key, which is the hashed name of the metadata format.</param>
        /// <param name="subRangeBegin">An iterator to the first position of the sub-range.</param>
        /// <param name="subRangeEnd">An iterator to one past the last position of the sub-range.</param>
        /// <returns>When a sub-range has been found, <c>true</c>, otherwise, <c>false</c>.</returns>
        bool GetSubRange(uint32_t searchKey,
                         VecOfItems::const_iterator &subRangeBegin,
                         VecOfItems::const_iterator &subRangeEnd) const
        {
            return utils::BinSearchSubRange(searchKey, subRangeBegin, subRangeEnd);
        }

    };// end of MetadataItems class


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
            ComPtr<IXMLDOMNodeList> nodeList;
            CHECK(dom->selectNodes(listXPath, &nodeList));

            const _bstr_t attrNameGuid(L"guid"),
                          attrNameName(L"name");

            // Iterate over list of container formats:
            
            long nodeCount;
            CHECK(nodeList->get_length(&nodeCount));
            for (long idx = 0; idx < nodeCount; ++idx)
            {
                ComPtr<IXMLDOMNode> node;
                CHECK(nodeList->get_item(idx, node.GetAddressOf()));

                ComPtr<IXMLDOMNamedNodeMap> attributes;
                CHECK(node->get_attributes(attributes.GetAddressOf()));

                // get GUID & name:

                GUID guid;
                _variant_t guidAsVariant;
                ComPtr<IXMLDOMNode> guidAttrNode;
                CHECK(attributes->getNamedItem(attrNameGuid, guidAttrNode.GetAddressOf()));
                CHECK(guidAttrNode->get_nodeValue(guidAsVariant.GetAddress()));
                _ASSERTE(guidAsVariant.vt == VT_BSTR);
                CHECK(IIDFromString(guidAsVariant.bstrVal, &guid));
                uint32_t hashedGuid = HashGuid(guid);

                uint32_t hashedName;
                GetAttributeValueHash(attributes.Get(), attrNameName, hashedName);

                // check whether GUID is unique in list:
                if (!nameByGuid.emplace(hashedGuid, hashedName).second)
                {
                    _bstr_t xmlSource;
                    node->get_xml(xmlSource.GetAddress());

                    std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                    std::wostringstream woss;
                    woss << L"Configuration file cannot have duplicated GUID in list of formats: ocurred in " << xmlSource.GetBSTR();
                    throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                }

                // check whether name is unique in list:
                if (!guidByName.emplace(hashedName, hashedGuid).second)
                {
                    _bstr_t xmlSource;
                    node->get_xml(xmlSource.GetAddress());
                    
                    std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                    std::wostringstream woss;
                    woss << L"Configuration file cannot have duplicated name in list of formats: ocurred in " << xmlSource.GetBSTR();
                    throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                }
            }
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions for errors known to have been already handled
        }
        catch (_com_error &ex)
        {
            WWAPI::RaiseHResultException(ex, "Failed to read formats from configuration");
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
        : m_mapCases(nullptr)
        , m_items(nullptr)
    {
        CALL_STACK_TRACE;
        
        try
        {
            // Instantiante the DOM parser:

            ComPtr<IXMLDOMDocument2> dom;
            auto hr = CoCreateInstance(__uuidof(DOMDocument60),
                                       nullptr,
                                       CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(dom.GetAddressOf()));
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Could not instantiate MSXML6 DOM document parser", "CoCreateInstance");

            dom->put_async(VARIANT_FALSE);
            dom->put_validateOnParse(VARIANT_TRUE);
            dom->put_resolveExternals(VARIANT_TRUE);

            // Parse the XML document:

            VARIANT_BOOL parserSucceeded;
            hr = dom->load(_variant_t(cfgFilePath.c_str()), &parserSucceeded);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to load XML configuration file into DOM parser", "IXMLDOMDocument2::load");

            if (parserSucceeded == VARIANT_FALSE)
            {
                long lineNumber;
                _bstr_t reason, xmlSource;
                ComPtr<IXMLDOMParseError> parseError;
                dom->get_parseError(parseError.GetAddressOf());
                parseError->get_reason(reason.GetAddress());
                parseError->get_srcText(xmlSource.GetAddress());
                parseError->get_line(&lineNumber);

                std::wostringstream woss;
                woss << L"Failed to parse configuration file! " << reason.GetBSTR()
                     << L" - at line " << lineNumber
                     << L": " << xmlSource.GetBSTR();

                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
            }
            
            CHECK(dom->setProperty(_bstr_t(L"SelectionLanguage"), _variant_t(_bstr_t(L"XPath"))));
            CHECK(dom->setProperty(_bstr_t(L"SelectionNamespaces"), _variant_t(_bstr_t(L"xmlns:def='http://3fd.codeplex.com/MetadataCopyMap.xsd'"))));

            Hash2HashMap containerFormatByName, containerFormatByGuid;
            ReadFormats(dom.Get(), _bstr_t(L"//def:metadata/def:formats/def:container/*"), containerFormatByGuid, containerFormatByName);

            Hash2HashMap metadataFormatByName, metadataFormatByGuid;
            ReadFormats(dom.Get(), _bstr_t(L"//def:metadata/def:formats/def:metadata/*"), metadataFormatByGuid, metadataFormatByName);

            std::unique_ptr<MetadataMapCases> metadataMapCases(
                new MetadataMapCases(dom.Get(),
                                     _bstr_t(L"//def:metadata/def:map/*"),
                                     containerFormatByName,
                                     metadataFormatByName)
            );

            std::unique_ptr<MetadataItems> metadataItems(
                new MetadataItems(dom.Get(),
                                  _bstr_t(L"//def:metadata/def:items/*"),
                                  metadataFormatByName)
            );

            m_mapCases = metadataMapCases.release();
            m_items = metadataItems.release();
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions for errors known to have been already handled
        }
        catch (_com_error &ex)
        {
            WWAPI::RaiseHResultException(ex, "Failed when loading configuration");
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when loading configuration: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="MetadataCopier"/> class.
    /// </summary>
    MetadataCopier::~MetadataCopier()
    {
        delete m_items;
        delete m_mapCases;
    }

}// end of namespace application
