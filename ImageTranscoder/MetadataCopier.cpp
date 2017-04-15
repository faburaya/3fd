#include "stdafx.h"
#include "MetadataCopier.h"
#include "WicUtilities.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include "3FD\utils_algorithms.h"
#include <wincodecsdk.h>
#include <MsXml6.h>
#include <map>
#include <set>
#include <array>
#include <sstream>
#include <codecvt>
#include <algorithm>
#include <cassert>

/* XML parsing validates the content against the referenced schema (XSD), thus the calls for
   browsing the DOM are not supposed to fail. Their results are only checked because failures
   such as running out of memory can always happen. */

namespace application
{
    using namespace _3fd;
    using namespace _3fd::core;

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

        /// <summary>
        /// Counts this how many map cases have been loaded from configuration file.
        /// </summary>
        /// <returns>The number of map cases.</returns>
        uint16_t Count() const
        {
            return m_mapCasesEntries.size();
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

        /// <summary>
        /// Counts how manu metadata items have been loaded from the configuration file.
        /// </summary>
        /// <returns>The number of metadata items.</returns>
        uint16_t Count() const
        {
            return m_items.size();
        }

    };// end of MetadataItems class


    ////////////////////////////
    // MetadataCopier Class
    ////////////////////////////

    std::mutex MetadataCopier::singletonCreationMutex;

    std::unique_ptr<MetadataCopier> MetadataCopier::uniqueInstance;

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

            }// for loop end
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
            auto hr = CoCreateInstance(CLSID_WICImagingFactory,
                                       nullptr,
                                       CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(m_wicImagingFactory.GetAddressOf()));
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to create imaging factory", "CoCreateInstance");

            // Instantiante the DOM parser:

            ComPtr<IXMLDOMDocument2> dom;
            hr = CoCreateInstance(__uuidof(DOMDocument60),
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

            std::ostringstream oss;
            oss << "Finished loading configurations from file.\nSupporting " << containerFormatByName.size()
                << " container formats and " << metadataFormatByName.size() << " metadata formats.\nLoaded "
                << m_mapCases->Count() << " map cases and " << m_items->Count() << " common items.";

            Logger::Write(oss.str(), Logger::PRIO_DEBUG);
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

    /// <summary>
    /// Finalizes the singleton.
    /// </summary>
    void MetadataCopier::Finalize()
    {
        uniqueInstance.reset();
    }

    /// <summary>
    /// Gets the singleton instance.
    /// </summary>
    /// <returns>A reference to the initialized singleton.</returns>
    MetadataCopier & MetadataCopier::GetInstance()
    {
        if (uniqueInstance)
            return *uniqueInstance;

        CALL_STACK_TRACE;

        try
        {
            std::lock_guard<std::mutex> lock(singletonCreationMutex);

            if (!uniqueInstance)
                uniqueInstance.reset(new MetadataCopier("MetadataCopyMap.xml"));

            return *uniqueInstance;
        }
        catch (IAppException &)
        {
            throw; // just forward exceptions for errors known to have been already handled
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "Failed to instantiate the metadata copier: " << core::StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when instantiating the metadata copier: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Copies all metadata items under the current path of the query reader to the query writer.
    /// </summary>
    /// <param name="embQueryReader">The embedded metadata query reader.</param>
    /// <param name="embQueryWriter">The embedded metadata query writer.</param>
    static void CopyAllItems(IWICMetadataQueryReader *embQueryReader, IWICMetadataQueryWriter *embQueryWriter)
    {
        CALL_STACK_TRACE;

        ComPtr<IEnumString> stringEnumerator;
        auto hr = embQueryReader->GetEnumerator(stringEnumerator.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to get enumerator of metadata query strings", "IWICMetadataQueryReader::GetEnumerator");

        ULONG numFetched;
        std::array<LPOLESTR, 4096 / sizeof(void *)> queryStrings;
        
        do
        {
            hr = stringEnumerator->Next(queryStrings.size(), queryStrings.data(), &numFetched);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to read query strings from enumerator", "IEnumString::Next");

            for (int idx = 0; idx < numFetched; ++idx)
            {
                _propvariant_t propVar;
                auto query = queryStrings[idx];

                hr = embQueryReader->GetMetadataByName(query, &propVar);
                if (FAILED(hr))
                {
                    std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                    std::wostringstream woss;
                    woss << L"Failed to get metadata item '" << query << L"' from embedded query reader";
                    WWAPI::RaiseHResultException(hr, strConv.to_bytes(woss.str()).c_str(), "IWICMetadataQueryReader::GetMetadataByName");
                }

                hr = embQueryWriter->SetMetadataByName(query, &propVar);
                if (FAILED(hr))
                {
                    std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                    std::wostringstream woss;
                    woss << L"Failed to set metadata item '" << query << L"' into embedded query writer";
                    WWAPI::RaiseHResultException(hr, strConv.to_bytes(woss.str()).c_str(), "IWICMetadataQueryWriter::SetMetadataByName");
                }
            }

        } while (queryStrings.size() == numFetched);
    }

    /// <summary>
    /// Copies selected metadata items under the current path of the query reader to the query writer.
    /// </summary>
    /// <param name="embQueryReader">The embedded metadata query reader.</param>
    /// <param name="embQueryWriter">The embedded metadata query writer.</param>
    /// <param name="beginItem">An iterator to the first entry in a list describing the selected items.</param>
    /// <param name="endItem">An iterator to one position past the last item in a list describing the selected items.</param>
    static void CopySelectedItems(IWICMetadataQueryReader *embQueryReader,
                                  IWICMetadataQueryWriter *embQueryWriter,
                                  const VecOfItems::const_iterator &beginItem,
                                  const VecOfItems::const_iterator &endItem)
    {
        CALL_STACK_TRACE;

        std::for_each(beginItem, endItem, [embQueryReader, embQueryWriter](const ItemEntry &entry)
        {
            _propvariant_t propVar;
            wchar_t query[64];
            swprintf(query, L"/{ushort=%u}", entry.id);

            auto hr = embQueryReader->GetMetadataByName(query, &propVar);
            if (FAILED(hr))
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                std::wostringstream woss;
                woss << L"Failed to get metadata item '" << entry.name
                     << L"' (id = " << entry.id
                     << L", query = '" << query
                     << L"') from embedded query reader";

                WWAPI::RaiseHResultException(hr, strConv.to_bytes(woss.str()).c_str(), "IWICMetadataQueryReader::GetMetadataByName");
            }

            hr = embQueryWriter->SetMetadataByName(query, &propVar);
            if (FAILED(hr))
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                std::wostringstream woss;
                woss << L"Failed to set metadata item '" << entry.name
                     << L"' (id = " << entry.id
                     << L", query = '" << query
                     << L"') into embedded query writer";

                WWAPI::RaiseHResultException(hr, strConv.to_bytes(woss.str()).c_str(), "IWICMetadataQueryWriter::SetMetadataByName");
            }
        });
    }

    /// <summary>
    /// Copies metadata, as configured, from the decoder query reader to the encoder query writer.
    /// </summary>
    /// <param name="from">The metadata query reader retrieved from the decoder.</param>
    /// <param name="to">The metadata query writer retrieved from the encoder.</param>
    void MetadataCopier::Copy(IWICMetadataQueryReader *from, IWICMetadataQueryWriter *to)
    {
        CALL_STACK_TRACE;

        GUID srcFormat;
        auto hr = from->GetContainerFormat(&srcFormat);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to retrieve container format", "IWICMetadataQueryReader::GetContainerFormat");

        GUID destFormat;
        hr = to->GetContainerFormat(&destFormat);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to retrieve container format", "IWICMetadataQueryWriter::GetContainerFormat");

        /* Look for the metadata map case that goes from the
           original container format to the destination format: */

        VecOfCaseEntries::const_iterator beginMap, endMap;
        auto mapCaseKey = MakeKey(srcFormat, destFormat);

        if (!m_mapCases->GetSubRange(mapCaseKey, beginMap, endMap))
            return;

        // iterate over the map case entries:
        std::for_each(beginMap, endMap, [this, from, to](const MapCaseEntry &entry)
        {
            // get embedded query reader for specific metadata format:

            _propvariant_t readerPropVar;
            auto hr = from->GetMetadataByName(entry.fromPath, &readerPropVar);
            if (FAILED(hr))
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                std::wostringstream woss;
                woss << L"Failed to retrieve reader for metadata in path " << entry.fromPath;

                WWAPI::RaiseHResultException(hr,
                    strConv.to_bytes(woss.str()).c_str(),
                    "IWICMetadataQueryReader::GetMetadataByName");
            }

            _ASSERTE(readerPropVar.vt == VT_UNKNOWN);
            ComPtr<IWICMetadataQueryReader> embQueryReader;
            hr = readerPropVar.punkVal->QueryInterface(IID_PPV_ARGS(embQueryReader.GetAddressOf()));
            if (FAILED(hr))
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                std::wostringstream woss;
                woss << L"Failed to get interface for embedded query reader of metadata in path " << entry.fromPath;

                WWAPI::RaiseHResultException(hr,
                    strConv.to_bytes(woss.str()).c_str(),
                    "IUnknown::QueryInterface");
            }

            // create embedded query writer for same metadata format:

            ComPtr<IWICMetadataQueryWriter> embQueryWriter;
            hr = m_wicImagingFactory->CreateQueryWriterFromReader(embQueryReader.Get(),
                                                                  nullptr,
                                                                  embQueryWriter.GetAddressOf());
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to create metadata query writer from reader info",
                    "IWICImagingFactory::CreateQueryWriterFromReader");
            }

            // copy the items:

            if (entry.onlyCommon)
            {
                VecOfItems::const_iterator beginItem, endItem;

                if (m_items->GetSubRange(entry.metaFmtNameHash, beginItem, endItem))
                {
                    CopySelectedItems(embQueryReader.Get(),
                                      embQueryWriter.Get(),
                                      beginItem,
                                      endItem);
                }
            }
            else
                CopyAllItems(embQueryReader.Get(), embQueryWriter.Get());

            _propvariant_t writerPropVar;
            writerPropVar.vt = VT_UNKNOWN;
            writerPropVar.punkVal = embQueryWriter.Get();
            writerPropVar.punkVal->AddRef();

            hr = to->SetMetadataByName(entry.toPath, &writerPropVar);
            if (FAILED(hr))
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                std::wostringstream woss;
                woss << L"Failed to write path '" << entry.toPath << L"' into metadata query writer";

                WWAPI::RaiseHResultException(hr,
                    strConv.to_bytes(woss.str()).c_str(),
                    "IWICMetadataQueryWriter::SetMetadataByName");
            }
            
        });// for_each loop end
    }

}// end of namespace application
