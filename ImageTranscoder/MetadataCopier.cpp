#include "stdafx.h"
#include "MetadataCopier.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "Poco/UUID.h"
#include <MsXml6.h>
#include <comutil.h>
#include <wrl.h>
#include <codecvt>

namespace application
{
    using namespace Microsoft::WRL;
    using namespace _3fd::core;

    /// <summary>
    /// Initializes a new instance of the <see cref="MetadataCopier"/> class.
    /// </summary>
    /// <param name="cfgFilePath">The path for the XML configuration file.</param>
    MetadataCopier::MetadataCopier(const string &cfgFilePath)
    {
        CALL_STACK_TRACE;
        
        // Instantiante the DOM parser:

        ComPtr<IXMLDOMDocument> dom;
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
        std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
        _variant_t varCfgFilePath(strConv.from_bytes(cfgFilePath).c_str());
        hr = dom->load(varCfgFilePath.GetVARIANT(), &parserSucceeded);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to load XML configuration file into DOM parser", "IXMLDOMDocument::load");

        if (parserSucceeded == VARIANT_FALSE)
        {
            _bstr_t reason;
            ComPtr<IXMLDOMParseError> parseError;
            dom->get_parseError(parseError.GetAddressOf());
            parseError->get_reason(reason.GetAddress());
            throw AppException<std::runtime_error>("Failed to parse XML configuration file", static_cast<const char *> (reason));
        }
    }

}// end of namespace application
