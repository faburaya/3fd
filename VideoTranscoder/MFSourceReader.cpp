#include "stdafx.h"
#include "MediaFoundationWrappers.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"

#include <iostream>
#include <algorithm>
#include <codecvt>
#include <d3d11.h>
#include <mfapi.h>

namespace application
{
    using namespace _3fd::core;

    // Helps creating an uncompressed video media type (based on original) for source reader
    static ComPtr<IMFMediaType> CreateUncompressedVideoMediaType(const ComPtr<IMFMediaType> &mfSrcEncVideoMType)
    {
        CALL_STACK_TRACE;

        HRESULT hr;

#ifndef NDEBUG
        // Get major type of input:
        GUID majorType = { 0 };
        hr = mfSrcEncVideoMType->GetMajorType(&majorType);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to retrieve major media type from original source media type",
                "IMFMediaType::GetMajorType"
            );
        }

        // This function only works with video!
        _ASSERTE(majorType != MFMediaType_Video);
#endif
        // Get subtype of input:
        GUID subType = { 0 };
        hr = mfSrcEncVideoMType->GetGUID(MF_MT_SUBTYPE, &subType);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to retrieve video subtype from original source media type",
                "IMFMediaType::GetGUID"
            );
        }

        ComPtr<IMFMediaType> mfUncompVideoMType;

        // Original subtype is already uncompressed:
        if (subType == MFVideoFormat_RGB32)
            return mfUncompVideoMType;

        // Create the uncompressed media type:
        hr = MFCreateMediaType(mfUncompVideoMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create uncompressed media type for source", "MFCreateMediaType");

        // Setup uncompressed media type mainly as a copy from original encoded version...
        hr = mfSrcEncVideoMType->CopyAllItems(mfUncompVideoMType.Get());
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to copy attributes from original source media type",
                "IMFMediaType::CopyAllItems"
            );
        }

        // Set uncompressed video format:

        hr = mfUncompVideoMType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set video format RGB32 on uncompressed media type for source",
                "IMFMediaType::SetGUID"
            );
        }

        hr = mfUncompVideoMType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set video media type as uncompressed",
                "IMFMediaType::SetUINT32"
            );
        }

        // Fix up PAR if not set on the original media type:

        MFRatio par = { 0 };
        hr = MFGetAttributeRatio(
            mfUncompVideoMType.Get(),
            MF_MT_PIXEL_ASPECT_RATIO,
            reinterpret_cast<UINT32 *> (&par.Numerator),
            reinterpret_cast<UINT32 *> (&par.Denominator)
        );

        if (FAILED(hr))
        {
            // Default to square pixels:
            hr = MFSetAttributeRatio(
                mfUncompVideoMType.Get(),
                MF_MT_PIXEL_ASPECT_RATIO,
                1, 1
            );

            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to set pixel aspect ratio of uncompressed media type for source",
                    "IMFMediaType::SetGUID"
                );
            }
        }

        return mfUncompVideoMType;
    }

    // Helps creating an uncompressed audio media type for source reader
    static ComPtr<IMFMediaType> CreateUncompressedAudioMediaType(const ComPtr<IMFMediaType> &mfSrcEncAudioMType)
    {
        CALL_STACK_TRACE;

        HRESULT hr;

#ifndef NDEBUG
        // Get major type of input:
        GUID majorType = { 0 };
        hr = mfSrcEncAudioMType->GetMajorType(&majorType);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to retrieve major media type from original source media type",
                "IMFMediaType::GetMajorType"
            );
        }

        // This function only works with audio!
        _ASSERTE(majorType != MFMediaType_Audio);
#endif
        // Get subtype of input:
        GUID subType = { 0 };
        hr = mfSrcEncAudioMType->GetGUID(MF_MT_SUBTYPE, &subType);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to retrieve audio subtype from original source media type",
                "IMFMediaType::GetGUID"
            );
        }

        ComPtr<IMFMediaType> mfUncompAudioMType;

        // Original subtype is already uncompressed:
        if (subType == MFAudioFormat_PCM)
            return mfUncompAudioMType;

        /* Get the sample rate and other information from the audio format.
           Note: Some encoded audio formats do not contain a value for bits/sample.
           In that case, use a default value of 16. Most codecs will accept this value. */

        auto nChannels = MFGetAttributeUINT32(mfSrcEncAudioMType.Get(), MF_MT_AUDIO_NUM_CHANNELS, 0);
        auto sampleRate = MFGetAttributeUINT32(mfSrcEncAudioMType.Get(), MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
        auto bitsPerSample = MFGetAttributeUINT32(mfSrcEncAudioMType.Get(), MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

        if (nChannels == 0 || sampleRate == 0)
        {
            throw AppException<std::runtime_error>(
                "Could not retrieve information to create uncompressed media type "
                "for source, because it was not available in original media type"
            );
        }

        // Calculate derived values:
        auto blockAlign = nChannels * (bitsPerSample / 8);
        auto bytesPerSecond = blockAlign * sampleRate;

        // Create an empty media type:
        hr = MFCreateMediaType(mfUncompAudioMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to uncompressed media type for source", "MFCreateMediaType");
        
        // Set attributes to make it PCM:
        if (FAILED(hr = mfUncompAudioMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
            FAILED(hr = mfUncompAudioMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
            FAILED(hr = mfUncompAudioMType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, nChannels)) ||
            FAILED(hr = mfUncompAudioMType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate)) ||
            FAILED(hr = mfUncompAudioMType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign)) ||
            FAILED(hr = mfUncompAudioMType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bytesPerSecond)) ||
            FAILED(hr = mfUncompAudioMType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample)) ||
            FAILED(hr = mfUncompAudioMType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE)))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set attribute in uncompressed media type for source",
                "IMFMediaType::SetGUID / SetUINT32"
            );
        }

        return mfUncompAudioMType;
    }

    // Helps obtaining a Direct3D device for Microsoft DXGI
    static ComPtr<ID3D11Device> GetDeviceDirect3D(UINT idxVideoAdapter)
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
                "IDXGIAdapter1::GetDesc1"
            );
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

        const auto nFeatLevels = static_cast<UINT> (
            (sizeof d3dFeatureLevels) / sizeof(D3D_FEATURE_LEVEL)
        );

        // Create Direct3D device:
        D3D_FEATURE_LEVEL featLevelCodeSuccess;
        ComPtr<ID3D11Device> d3dDx11Device;
        hr = D3D11CreateDevice(
            dxgiAdapter.Get(),
            D3D_DRIVER_TYPE_HARDWARE,
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
                D3D_DRIVER_TYPE_HARDWARE,
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
            [d3dFeatureLevels, featLevelCodeSuccess](const KeyValPair &entry)
            {
                return entry.code == featLevelCodeSuccess;
            }
        );

        std::cout << "Hardware device supports " << d3dFLNameKVPairIter->name << std::endl;
        
        return d3dDx11Device;
    }

    //////////////////////////////
    // Class MFSourceReader
    //////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="MFSourceReader"/> class.
    /// </summary>
    /// <param name="url">The URL.</param>
    /// <param name="idxVideoAdapter">The index of the video adapter whose device will be in use by DXVA.</param>
    MFSourceReader::MFSourceReader(const string &url, UINT idxVideoAdapter)
    try
    {
        CALL_STACK_TRACE;

        // Create an instance of the Microsoft DirectX Graphics Infrastructure (DXGI) Device Manager:
        ComPtr<IMFDXGIDeviceManager> mfDXGIDevMan;
        UINT dxgiResetToken;
        HRESULT hr = MFCreateDXGIDeviceManager(&dxgiResetToken, mfDXGIDevMan.GetAddressOf());
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to create Microsoft DXGI Device Manager object for media source reader",
                "MFCreateDXGIDeviceManager"
            );
        }

        // Get Direct3D device and associate with DXGI device manager:
        auto d3dDevice = GetDeviceDirect3D(idxVideoAdapter);
        mfDXGIDevMan->ResetDevice(d3dDevice.Get(), dxgiResetToken);
        
        // Create attributes store, to set properties on the source reader:
        ComPtr<IMFAttributes> mfAttrStore;
        hr = MFCreateAttributes(mfAttrStore.GetAddressOf(), 2);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create attributes store", "MFCreateAttributes");

        // enable DXVA encoding
        mfAttrStore->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, mfDXGIDevMan.Get());

        // enable codec hardware acceleration
        mfAttrStore->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto ucs2url = transcoder.from_bytes(url);

        hr = MFCreateSourceReaderFromURL(ucs2url.c_str(),
                                         mfAttrStore.Get(),
                                         m_mfSourceReader.GetAddressOf());

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create media source reader", "MFCreateSourceReaderFromURL");

        // Get the encoded video media type:
        ComPtr<IMFMediaType> mfSrcEncVideoMType;
        hr = m_mfSourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, mfSrcEncVideoMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to get video media type from source", "IMFSourceReader::GetNativeMediaType");

        // Set the source reader to use the new uncompressed video media type, if not already uncompressed:

        auto mfUncompVideoMType = CreateUncompressedVideoMediaType(mfSrcEncVideoMType);

        if (mfUncompVideoMType)
        {
            hr = m_mfSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, mfUncompVideoMType.Get());
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to set source reader output to uncompressed video media type",
                    "IMFSourceReader::SetCurrentMediaType"
                );
            }
        }
        
        // Get the encoded video media type:
        ComPtr<IMFMediaType> mfSrcEncAudioMType;
        hr = m_mfSourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, mfSrcEncAudioMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to get media type from source", "IMFSourceReader::GetNativeMediaType");

        // Set the source reader to use the new uncompressed audio media type, if not already uncompressed:

        auto mfUncompAudioMType = CreateUncompressedAudioMediaType(mfSrcEncAudioMType);

        if (mfUncompAudioMType)
        {
            hr = m_mfSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, mfUncompAudioMType.Get());
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to set source reader output to uncompressed audio media type",
                    "IMFSourceReader::SetCurrentMediaType"
                );
            }
        }
    }
    catch (IAppException &ex)
    {
        throw AppException<std::runtime_error>("Failed to create media source reader", ex);
    }
    catch (std::exception &ex)
    {
        std::ostringstream oss;
        oss << "Generic failure when creating media source reader: " << ex.what();
        throw AppException<std::runtime_error>(oss.str());
    }

}// end of namespace application
