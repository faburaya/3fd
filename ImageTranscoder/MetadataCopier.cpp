#include "stdafx.h"
#include "MetadataCopier.h"
#include "WicUtilities.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include "3FD\utils_algorithms.h"

#ifdef _3FD_PLATFORM_WIN32API
#   include <MsXml6.h>
#elif defined _3FD_PLATFORM_WINRT
#   include "3FD\utils_winrt.h"
#endif

#include <wincodecsdk.h>
#include <map>
#include <set>
#include <array>
#include <sstream>
#include <codecvt>
#include <algorithm>
#include <cassert>

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
    static uint64_t GetMetadataMapCaseKey(XmlNode elemNode, const Hash2HashMap &containerFormatByName)
    {
        CALL_STACK_TRACE;

        const XmlStrWrap attrNameFormats[] = { XmlStrWrap(L"srcFormat"), XmlStrWrap(L"destFormat") };

        std::array<uint32_t, sizeof attrNameFormats / sizeof attrNameFormats[0]> hashedFormatGUIDs;
        std::array<uint32_t, sizeof attrNameFormats / sizeof attrNameFormats[0]> hashedFormatNames;
        
        XmlNamedNodeMap attributes = XmlGetAttributes(elemNode);

        for (int idx = 0; idx < hashedFormatNames.size(); ++idx)
        {
            // get container format name
            auto formatName = GetAttributeValueHash(attributes, attrNameFormats[idx], hashedFormatNames[idx]);
            
            // does the format have a GUID?
            auto iter = containerFormatByName.find(hashedFormatNames[idx]);
            if (containerFormatByName.end() == iter)
            {
                auto xmlSource = XmlGetXml(elemNode);
                std::wostringstream woss;
                woss << L"Invalid setting in configuration file! Microsoft WIC GUID was not defined for container format "
                     << UnwrapCString(formatName) << L". Ocurred in:\r\n" << UnwrapCString(xmlSource);

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
        XMLSTR fromPath;
        XMLSTR toPath;
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
        MetadataMapCases(XmlDom dom,
                         XMLSTR mapCasesXPath,
                         const Hash2HashMap &containerFormatByName,
                         const Hash2HashMap &metadataFormatByName)
        {
            CALL_STACK_TRACE;

            try
            {
                std::set<decltype(MapCaseEntry::key)> uniqueKeys;

                const XmlStrWrap attrNameMetaFormat(L"metaFormat"),
                                 attrNamePathFrom(L"fromPath"),
                                 attrNamePathTo(L"toPath"),
                                 attrNameOnlyCommon(L"onlyCommon");

                XmlNodeList listOfMapCaseNodes = XmlSelectNodes(dom, mapCasesXPath);
                auto mapCaseNodesCount = XmlGetLength(listOfMapCaseNodes);

                std::vector<MapCaseEntry> mapCasesEntries;
                mapCasesEntries.reserve(mapCaseNodesCount);

                // Iterate over list of map cases:

                for (decltype(mapCaseNodesCount) idxCase = 0; idxCase < mapCaseNodesCount; ++idxCase)
                {
                    XmlNode mapCaseNode = XmlGetItem(listOfMapCaseNodes, idxCase);
                    auto mapCaseKey = GetMetadataMapCaseKey(mapCaseNode, containerFormatByName);
                    
                    // is map case unique?
                    if (!uniqueKeys.insert(mapCaseKey).second)
                    {
                        std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                        std::wostringstream woss;
                        auto xmlSource = XmlGetXml(mapCaseNode);
                        woss << L"Configuration file cannot have duplicated metadata map cases! Ocurred in:\r\n" << UnwrapCString(xmlSource);
                        throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                    }

                    // Iterate over list entries:

                    XmlNodeList listOfEntryNodes = XmlGetChildNodes(mapCaseNode);
                    auto entryNodesCount = XmlGetLength(listOfEntryNodes);

                    for (decltype(entryNodesCount) idxEntry = 0; idxEntry < entryNodesCount; ++idxEntry)
                    {
                        mapCasesEntries.emplace_back();
                        auto &newEntry = mapCasesEntries.back();
                        newEntry.key = mapCaseKey;

                        XmlNode entryNode = XmlGetItem(listOfEntryNodes, idxEntry);
                        XmlNamedNodeMap attributes = XmlGetAttributes(entryNode);

                        // get metadata format
                        auto metaFormatName = GetAttributeValueHash(attributes, attrNameMetaFormat, newEntry.metaFmtNameHash);

                        // does the format have a GUID?
                        if (metadataFormatByName.find(newEntry.metaFmtNameHash) == metadataFormatByName.end())
                        {
                            auto xmlSource = XmlGetXml(mapCaseNode);
                            std::wostringstream woss;
                            woss << L"Invalid setting in configuration file! Microsoft WIC GUID was not defined for metadata format "
                                 << UnwrapCString(metaFormatName) << L". Ocurred in:\r\n" << UnwrapCString(xmlSource);

                            std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                            throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                        }

                        GetAttributeValue(attributes, attrNamePathFrom, newEntry.fromPath);
                        GetAttributeValue(attributes, attrNamePathTo, newEntry.toPath);
                        GetAttributeValue(attributes, attrNameOnlyCommon, newEntry.onlyCommon);

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
#       ifdef _3FD_PLATFORM_WIN32API
            catch (_com_error &ex)
            {
                WWAPI::RaiseHResultException(ex, "Failed to read metadata map cases from configuration");
            }
#       elif defined _3FD_PLATFORM_WINRT
            catch (Platform::Exception ^ex)
            {
                std::ostringstream oss;
                oss << "Failed to read metadata map cases from configuration: " << WWAPI::GetDetailsFromWinRTEx(ex);
                throw AppException<std::runtime_error>(oss.str());
            }
#       endif
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
#       ifdef _3FD_PLATFORM_WIN32API
            for (auto &entry : m_mapCasesEntries)
            {
                SysFreeString(entry.fromPath);
                SysFreeString(entry.toPath);
            }
#       endif
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
            subRangeEnd = m_mapCasesEntries.end();
            subRangeBegin = m_mapCasesEntries.begin();
            return utils::BinSearchSubRange(searchKey, subRangeBegin, subRangeEnd);
        }

        /// <summary>
        /// Counts this how many map cases have been loaded from configuration file.
        /// </summary>
        /// <returns>The number of map cases.</returns>
        uint16_t Count() const
        {
            return static_cast<uint16_t> (m_mapCasesEntries.size());
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
        XMLSTR name;

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
        MetadataItems(XmlDom dom,
                      XMLSTR itemsXPath,
                      const Hash2HashMap &metadataFormatByName)
        {
            CALL_STACK_TRACE;

            try
            {
                std::set<decltype(ItemEntry::id)> uniqueKeys;

                const XmlStrWrap attrNameId(L"id"),
                                 attrNameMetaFormat(L"metaFormat"),
                                 attrNameRational(L"rational"),
                                 attrNameName(L"name");

                XmlNodeList listOfNodes = XmlSelectNodes(dom, itemsXPath);
                auto nodesCount = XmlGetLength(listOfNodes);

                std::vector<ItemEntry> items;
                items.reserve(nodesCount);

                // Iterate over list of items:

                for (decltype(nodesCount) idx = 0; idx < nodesCount; ++idx)
                {
                    items.emplace_back();
                    auto &newEntry = items.back();

                    XmlNode elemNode = XmlGetItem(listOfNodes, idx);
                    XmlNamedNodeMap attributes = XmlGetAttributes(elemNode);

                    // get item ID
                    GetAttributeValue(attributes, attrNameId, newEntry.id);

                    // is the item ID unique?
                    if (!uniqueKeys.insert(newEntry.id).second)
                    {
                        std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                        auto xmlSource = XmlGetXml(elemNode);
                        std::wostringstream woss;
                        woss << L"Configuration file cannot have duplicated metadata items! Ocurred in:\r\n" << UnwrapCString(xmlSource);
                        throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                    }

                    // get metadata format
                    auto metaFormatName = GetAttributeValueHash(attributes, attrNameMetaFormat, newEntry.metaFmtNameHash);

                    // does the format have a GUID?
                    if (metadataFormatByName.find(newEntry.metaFmtNameHash) == metadataFormatByName.end())
                    {
                        auto xmlSource = XmlGetXml(elemNode);
                        std::wostringstream woss;
                        woss << L"Invalid setting in configuration file! Microsoft WIC GUID was not defined for metadata format "
                             << UnwrapCString(metaFormatName) << L". Ocurred in:\r\n" << UnwrapCString(xmlSource);

                        std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                        throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                    }

                    GetAttributeValue(attributes, attrNameRational, newEntry.rational);
                    GetAttributeValue(attributes, attrNameName, newEntry.name);

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
#       ifdef _3FD_PLATFORM_WIN32API
            catch (_com_error &ex)
            {
                WWAPI::RaiseHResultException(ex, "Failed to read metadata items from configuration");
            }
#       elif defined _3FD_PLATFORM_WINRT
            catch (Platform::Exception ^ex)
            {
                std::ostringstream oss;
                oss << "Failed to read metadata items from configuration: " << WWAPI::GetDetailsFromWinRTEx(ex);
                throw AppException<std::runtime_error>(oss.str());
            }
#       endif
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
#       ifdef _3FD_PLATFORM_WIN32API
            for (auto &entry : m_items)
            {
                SysFreeString(entry.name);
            }
#       endif
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
            subRangeEnd = m_items.end();
            subRangeBegin = m_items.begin();
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
    static void ReadFormats(XmlDom dom,
                            XMLSTR listXPath,
                            Hash2HashMap &nameByGuid,
                            Hash2HashMap &guidByName)
    {
        CALL_STACK_TRACE;

        try
        {
            XmlNodeList nodeList = XmlSelectNodes(dom, listXPath);
            
            const XmlStrWrap attrNameGuid(L"guid"),
                             attrNameName(L"name");

            // Iterate over list of container formats:
            
            auto nodeCount = XmlGetLength(nodeList);
            for (decltype(nodeCount) idx = 0; idx < nodeCount; ++idx)
            {
                XmlNode node = XmlGetItem(nodeList, idx);
                XmlNamedNodeMap attributes = XmlGetAttributes(node);

                // get GUID & name:

                XmlNode guidAttrNode = XmlGetNamedItem(attributes, attrNameGuid);
                auto guidString = XmlGetNodeValue(guidAttrNode);

                HRESULT hr;
                GUID guid;
                CHECK(IIDFromString(UnwrapCString(guidString), &guid));

                uint32_t hashedGuid = HashGuid(guid);
                uint32_t hashedName;
                GetAttributeValueHash(attributes, attrNameName, hashedName);

                // check whether GUID is unique in list:
                if (!nameByGuid.emplace(hashedGuid, hashedName).second)
                {
                    std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                    auto xmlSource = XmlGetXml(node);
                    std::wostringstream woss;
                    woss << L"Configuration file cannot have duplicated GUID in list of formats: ocurred in " << UnwrapCString(xmlSource);
                    throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
                }

                // check whether name is unique in list:
                if (!guidByName.emplace(hashedName, hashedGuid).second)
                {
                    std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                    auto xmlSource = XmlGetXml(node);
                    std::wostringstream woss;
                    woss << L"Configuration file cannot have duplicated name in list of formats: ocurred in " << UnwrapCString(xmlSource);
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
#   ifdef _3FD_PLATFORM_WINRT
        catch (Platform::Exception ^ex)
        {
            std::ostringstream oss;
            oss << "Failed to read formats from configuration: " << WWAPI::GetDetailsFromWinRTEx(ex);
            throw AppException<std::runtime_error>(oss.str());
        }
#   endif
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

#   ifdef _3FD_PLATFORM_WIN32API

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
                     << L"At line " << lineNumber
                     << L": " << xmlSource.GetBSTR();

                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()));
            }
            
            CHECK(dom->setProperty(_bstr_t(L"SelectionLanguage"), _variant_t(_bstr_t(L"XPath"))));
            CHECK(dom->setProperty(_bstr_t(L"SelectionNamespaces"), _variant_t(_bstr_t(L"xmlns:tns='http://3fd.codeplex.com/MetadataCopyMap.xsd'"))));

#   elif defined _3FD_PLATFORM_WINRT

            auto configFile = utils::WinRTExt::WaitForAsync(
                Windows::ApplicationModel::Package::Current->InstalledLocation->GetFileAsync(L"MetadataCopyMap.xml")
            );

            auto dom = utils::WinRTExt::WaitForAsync(
                Windows::Data::Xml::Dom::XmlDocument::LoadFromFileAsync(configFile)
            );
#   endif
            Hash2HashMap containerFormatByName, containerFormatByGuid;
            ReadFormats(dom, XmlStrWrap(L"//tns:metadata/tns:formats/tns:container/*"), containerFormatByGuid, containerFormatByName);

            Hash2HashMap metadataFormatByName, metadataFormatByGuid;
            ReadFormats(dom, XmlStrWrap(L"//tns:metadata/tns:formats/tns:metadata/*"), metadataFormatByGuid, metadataFormatByName);

            std::unique_ptr<MetadataMapCases> metadataMapCases(
                new MetadataMapCases(dom,
                                     XmlStrWrap(L"//tns:metadata/tns:map/*"),
                                     containerFormatByName,
                                     metadataFormatByName)
            );

            std::unique_ptr<MetadataItems> metadataItems(
                new MetadataItems(dom,
                                  XmlStrWrap(L"//tns:metadata/tns:items/*"),
                                  metadataFormatByName)
            );

            m_mapCases = metadataMapCases.release();
            m_items = metadataItems.release();

            std::ostringstream oss;
            oss << "Finished loading configurations from file.\nSupporting " << containerFormatByName.size()
                << " container formats and " << metadataFormatByName.size() << " metadata formats.\nLoaded "
                << m_mapCases->Count() << " map cases and " << m_items->Count() << " common items.\n";

            Logger::Write(oss.str(), Logger::PRIO_DEBUG);
        }
        catch (_com_error &ex)
        {
            WWAPI::RaiseHResultException(ex, "Failed when loading configuration");
        }
#   ifdef _3FD_PLATFORM_WINRT
        catch (Platform::Exception ^ex)
        {
            std::ostringstream oss;
            oss << "Failed when loading configuration: " << core::WWAPI::GetDetailsFromWinRTEx(ex);
            throw AppException<std::runtime_error>(oss.str());
        }
#   endif
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

                /* If the item is in fact a group of sub-items, then it cannot be copied like this.
                   Instead, such group would have to be explicitly listed in MetadataCopyMap.xml. */
                if (propVar.vt == VT_UNKNOWN)
                    continue;

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
            wchar_t query[16];
            swprintf(query, sizeof query, L"/{ushort=%u}", entry.id);

            auto hr = embQueryReader->GetMetadataByName(query, &propVar);
            
            if (hr == WINCODEC_ERR_PROPERTYNOTFOUND)
                return;

            if (FAILED(hr))
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                std::wostringstream woss;
                woss << L"Failed to get metadata item '" << UnwrapCString(entry.name)
                     << L"' (id = " << entry.id
                     << L", query = '" << query
                     << L"') from embedded query reader";

                WWAPI::RaiseHResultException(hr, strConv.to_bytes(woss.str()).c_str(), "IWICMetadataQueryReader::GetMetadataByName");
            }

            if (propVar.vt == VT_EMPTY || propVar.vt == VT_NULL)
                return;

            hr = embQueryWriter->SetMetadataByName(query, &propVar);
            if (FAILED(hr))
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                std::wostringstream woss;
                woss << L"Failed to set metadata item '" << UnwrapCString(entry.name)
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

        auto mapCaseKey = MakeKey(srcFormat, destFormat);

        VecOfCaseEntries::const_iterator beginMap, endMap;
        if (!m_mapCases->GetSubRange(mapCaseKey, beginMap, endMap))
            return;

        // iterate over the map case entries:
        std::for_each(beginMap, endMap, [this, from, to](const MapCaseEntry &entry)
        {
            // get embedded query reader for specific metadata format:

            _propvariant_t readerPropVar;
            auto hr = from->GetMetadataByName(UnwrapCString(entry.fromPath), &readerPropVar);

            if (hr == WINCODEC_ERR_PROPERTYNOTFOUND)
                return;

            if (FAILED(hr))
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                std::wostringstream woss;
                woss << L"Failed to retrieve reader for metadata in path " << UnwrapCString(entry.fromPath);

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
                woss << L"Failed to get interface for embedded query reader of metadata in path " << UnwrapCString(entry.fromPath);

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
            try
            {
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
            }
            catch (IAppException &ex)
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                std::wostringstream woss;
                woss << L"Failed to copy metadata from " << UnwrapCString(entry.fromPath);
                throw AppException<std::runtime_error>(strConv.to_bytes(woss.str()), ex);
            }

            _propvariant_t writerPropVar;
            writerPropVar.vt = VT_UNKNOWN;
            writerPropVar.punkVal = embQueryWriter.Get();
            //writerPropVar.punkVal->AddRef();

            hr = to->SetMetadataByName(UnwrapCString(entry.toPath), &writerPropVar);
            if (FAILED(hr))
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
                std::wostringstream woss;
                woss << L"Failed to write path '" << UnwrapCString(entry.toPath) << L"' into metadata query writer";

                WWAPI::RaiseHResultException(hr,
                    strConv.to_bytes(woss.str()).c_str(),
                    "IWICMetadataQueryWriter::SetMetadataByName");
            }

        });// for_each loop end
    }

}// end of namespace application
