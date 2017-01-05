#include "stdafx.h"
#include "MediaFoundationWrappers.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include <iostream>
#include <algorithm>
#include <codecvt>
#include <mfapi.h>

namespace application
{
    using namespace _3fd::core;

    //////////////////////////////
    // Class MediaFoundationLib
    //////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="MediaFoundationLib"/> class.
    /// </summary>
    MediaFoundationLib::MediaFoundationLib()
    {
        CALL_STACK_TRACE;

        HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to initialize Microsoft Media Foundation platform", "MFStartup");
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="MediaFoundationLib"/> class.
    /// </summary>
    MediaFoundationLib::~MediaFoundationLib()
    {
        HRESULT hr = MFShutdown();

        if (FAILED(hr))
        {
            Logger::Write(hr,
                "Failed to shut down Microsoft Media Foundation platform",
                "MFShutdown",
                Logger::PRIO_CRITICAL);
        }
    }

    //////////////////////
    // Miscelaneous
    //////////////////////

    /// <summary>
    /// Helps obtaining a Direct3D device for Microsoft DXGI.
    /// </summary>
    /// <param name="idxVideoAdapter">The index of the video adapter whose device will be in use by DXVA.</param>
    /// <returns>The Direct3D device.</returns>
    ComPtr<ID3D11Device> GetDeviceDirect3D(UINT idxVideoAdapter)
    {
        CALL_STACK_TRACE;

        // Create DXGI factory:
        ComPtr<IDXGIFactory1> dxgiFactory;
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create DXGI factory", "CreateDXGIFactory1");

        // Get a video adapter:
        ComPtr<IDXGIAdapter1> dxgiAdapter;
        dxgiFactory->EnumAdapters1(idxVideoAdapter, dxgiAdapter.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to enumerate video adapters", "IDXGIAdapter1::EnumAdapters1");

        // Get video adapter description:
        DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
        dxgiAdapter->GetDesc1(&dxgiAdapterDesc);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to retrieve DXGI video adapter description",
                "IDXGIAdapter1::GetDesc1");
        }

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        std::cout << "Selected DXGI video adapter is \'"
            << transcoder.to_bytes(dxgiAdapterDesc.Description) << '\'' << std::endl;

        // Direct3D feature level codes and names:

        struct KeyValPair { int code; const char *name; };

        const KeyValPair d3dFLevelNames[] =
        {
            KeyValPair{ D3D_FEATURE_LEVEL_9_1, "Direct3D 9.1" },
            KeyValPair{ D3D_FEATURE_LEVEL_9_2, "Direct3D 9.2" },
            KeyValPair{ D3D_FEATURE_LEVEL_9_3, "Direct3D 9.3" },
            KeyValPair{ D3D_FEATURE_LEVEL_10_0, "Direct3D 10.0" },
            KeyValPair{ D3D_FEATURE_LEVEL_10_1, "Direct3D 10.1" },
            KeyValPair{ D3D_FEATURE_LEVEL_11_0, "Direct3D 11.0" },
            KeyValPair{ D3D_FEATURE_LEVEL_11_1, "Direct3D 11.1" },
        };

        // Feature levels for Direct3D support
        const D3D_FEATURE_LEVEL d3dFeatureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1,
        };

        constexpr auto nFeatLevels = static_cast<UINT> ((sizeof d3dFeatureLevels) / sizeof(D3D_FEATURE_LEVEL));

        // Create Direct3D device:
        D3D_FEATURE_LEVEL featLevelCodeSuccess;
        ComPtr<ID3D11Device> d3dDx11Device;
        hr = D3D11CreateDevice(
            dxgiAdapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
            d3dFeatureLevels,
            nFeatLevels,
            D3D11_SDK_VERSION,
            d3dDx11Device.GetAddressOf(),
            &featLevelCodeSuccess,
            nullptr
        );

        // Might have failed for lack of Direct3D 11.1 runtime:
        if (hr == E_INVALIDARG)
        {
            // Try again without Direct3D 11.1:
            hr = D3D11CreateDevice(
                dxgiAdapter.Get(),
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                d3dFeatureLevels + 1,
                nFeatLevels - 1,
                D3D11_SDK_VERSION,
                d3dDx11Device.GetAddressOf(),
                &featLevelCodeSuccess,
                nullptr
            );
        }

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create Direct3D device", "D3D11CreateDevice");

        // Get name of Direct3D feature level that succeeded upon device creation:
        auto d3dFLNameKVPairIter = std::find_if(
            d3dFLevelNames,
            d3dFLevelNames + nFeatLevels,
            [featLevelCodeSuccess](const KeyValPair &entry)
            {
                return entry.code == featLevelCodeSuccess;
            }
        );

        std::cout << "Hardware device supports " << d3dFLNameKVPairIter->name << std::endl;

        return d3dDx11Device;
    }

    /// <summary>
    /// Translates an MFT category into a label.
    /// </summary>
    /// <param name="transformCategory">The given transform category.</param>
    /// <returns>A string label for the MFT category.</returns>
    const char *TranslateMFTCategory(const GUID &transformCategory)
    {
        struct KeyValPair { GUID key; const char *value; };

        static const KeyValPair mftCatLabels[] =
        {
            KeyValPair{ MFT_CATEGORY_MULTIPLEXER,     "multiplexer" },
            KeyValPair{ MFT_CATEGORY_VIDEO_EFFECT,    "video effects" },
            KeyValPair{ MFT_CATEGORY_VIDEO_PROCESSOR, "video processor" },
            KeyValPair{ MFT_CATEGORY_OTHER,           "other" },
            KeyValPair{ MFT_CATEGORY_AUDIO_ENCODER,   "audio encoder" },
            KeyValPair{ MFT_CATEGORY_AUDIO_DECODER,   "audio decoder" },
            KeyValPair{ MFT_CATEGORY_AUDIO_EFFECT,    "audio effects" },
            KeyValPair{ MFT_CATEGORY_DEMULTIPLEXER,   "demultiplexer" },
            KeyValPair{ MFT_CATEGORY_VIDEO_DECODER,   "video decoder" },
            KeyValPair{ MFT_CATEGORY_VIDEO_ENCODER,   "video encoder" }
        };

        auto mftCatLblEnd = mftCatLabels + (sizeof mftCatLabels / sizeof(KeyValPair));

        auto mftCatLblIter = std::find_if(mftCatLabels, mftCatLblEnd,
            [&transformCategory](const KeyValPair &entry) { return entry.key == transformCategory; }
        );

        return (mftCatLblIter != mftCatLblEnd ? mftCatLblIter->value : "unknown");
    }

}// end of namespace application
