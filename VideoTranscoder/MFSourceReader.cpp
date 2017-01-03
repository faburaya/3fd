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
    static ComPtr<IMFMediaType> CreateUncompressedVideoMediaTypeFrom(const ComPtr<IMFMediaType> &srcEncVideoMType)
    {
        CALL_STACK_TRACE;

        HRESULT hr;

#ifndef NDEBUG
        // Get major type of input:
        GUID majorType = { 0 };
        hr = srcEncVideoMType->GetMajorType(&majorType);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to retrieve major media type from original source media type",
                "IMFMediaType::GetMajorType"
            );
        }

        // This function only works with video!
        _ASSERTE(majorType == MFMediaType_Video);
#endif
        ComPtr<IMFMediaType> uncompVideoMType;

        // Create the uncompressed media type:
        hr = MFCreateMediaType(uncompVideoMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create uncompressed media type for source", "MFCreateMediaType");

        // Setup uncompressed media type mainly as a copy from original encoded version...
        hr = srcEncVideoMType->CopyAllItems(uncompVideoMType.Get());
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to copy attributes from original source media type",
                "IMFMediaType::CopyAllItems"
            );
        }

        // Set uncompressed video format:

        hr = uncompVideoMType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set video format YUY2 on uncompressed media type for source",
                "IMFMediaType::SetGUID"
            );
        }

        hr = uncompVideoMType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
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
            uncompVideoMType.Get(),
            MF_MT_PIXEL_ASPECT_RATIO,
            reinterpret_cast<UINT32 *> (&par.Numerator),
            reinterpret_cast<UINT32 *> (&par.Denominator)
        );

        if (FAILED(hr))
        {
            // Default to square pixels:
            hr = MFSetAttributeRatio(
                uncompVideoMType.Get(),
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

        return uncompVideoMType;
    }

    // Helps creating an uncompressed audio media type for source reader
    static ComPtr<IMFMediaType> CreateUncompressedAudioMediaTypeFrom(const ComPtr<IMFMediaType> &srcEncAudioMType)
    {
        CALL_STACK_TRACE;

        HRESULT hr;

#ifndef NDEBUG
        // Get major type of input:
        GUID majorType = { 0 };
        hr = srcEncAudioMType->GetMajorType(&majorType);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to retrieve major media type from original source media type",
                "IMFMediaType::GetMajorType"
            );
        }

        // This function only works with audio!
        _ASSERTE(majorType == MFMediaType_Audio);
#endif
        // Get subtype of input:
        GUID subType = { 0 };
        hr = srcEncAudioMType->GetGUID(MF_MT_SUBTYPE, &subType);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to retrieve audio subtype from original source media type",
                "IMFMediaType::GetGUID"
            );
        }

        ComPtr<IMFMediaType> uncompAudioMType;

        // Original subtype is already uncompressed:
        if (subType == MFAudioFormat_PCM)
            return uncompAudioMType;

        /* Get the sample rate and other information from the audio format.
           Note: Some encoded audio formats do not contain a value for bits/sample.
           In that case, use a default value of 16. Most codecs will accept this value. */

        auto nChannels = MFGetAttributeUINT32(srcEncAudioMType.Get(), MF_MT_AUDIO_NUM_CHANNELS, 0);
        auto sampleRate = MFGetAttributeUINT32(srcEncAudioMType.Get(), MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
        auto bitsPerSample = MFGetAttributeUINT32(srcEncAudioMType.Get(), MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

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
        hr = MFCreateMediaType(uncompAudioMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to uncompressed media type for source", "MFCreateMediaType");
        
        // Set attributes to make it PCM:
        if (FAILED(hr = uncompAudioMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
            FAILED(hr = uncompAudioMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, nChannels)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bytesPerSecond)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE)))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set attribute in uncompressed media type for source",
                "IMFMediaType::SetGUID / SetUINT32"
            );
        }

        return uncompAudioMType;
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

        constexpr auto nFeatLevels = static_cast<UINT> (
            (sizeof d3dFeatureLevels) / sizeof(D3D_FEATURE_LEVEL)
        );

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

    ///////////////////////////////////////////
    // Source Reader Callback Implementation
    ///////////////////////////////////////////

    using namespace Microsoft::WRL::Wrappers;

    /// <summary>
    /// 
    /// </summary>
    /// <seealso cref="IMFSourceReaderCallback" />
    class MFSourceReaderCallbackImpl : public IMFSourceReaderCallback
    {
    private:

        long                m_nRefCount;
        CRITICAL_SECTION    m_critsec;
        HANDLE              m_hEvent;
        BOOL                m_bEOS;
        HRESULT             m_hrStatus;

        /// <summary>
        /// Finalizes an instance of the <see cref="MFSourceReaderCallbackImpl"/> class.
        /// </summary>
        /// <remarks>Destructor is private. Caller should call <see cref="Release"/>.</remarks>
        virtual ~MFSourceReaderCallbackImpl() {}

    public:

        MFSourceReaderCallbackImpl(HANDLE hEvent) :
            m_nRefCount(1), m_hEvent(hEvent), m_bEOS(FALSE), m_hrStatus(S_OK)
        {
            InitializeCriticalSection(&m_critsec);
        }

        // IUnknown methods
        STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
        {
            static const QITAB qit[] =
            {
                QITABENT(MFSourceReaderCallbackImpl, IMFSourceReaderCallback),
                { 0 },
            };
            return QISearch(this, qit, iid, ppv);
        }
        STDMETHODIMP_(ULONG) AddRef()
        {
            return InterlockedIncrement(&m_nRefCount);
        }
        STDMETHODIMP_(ULONG) Release()
        {
            ULONG uCount = InterlockedDecrement(&m_nRefCount);
            if (uCount == 0)
            {
                delete this;
            }
            return uCount;
        }

        // IMFSourceReaderCallback methods
        STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex,
            DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample);

        STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *)
        {
            return S_OK;
        }

        STDMETHODIMP OnFlush(DWORD)
        {
            return S_OK;
        }

    public:

        HRESULT Wait(DWORD dwMilliseconds, BOOL *pbEOS)
        {
            *pbEOS = FALSE;

            DWORD dwResult = WaitForSingleObject(m_hEvent, dwMilliseconds);
            if (dwResult == WAIT_TIMEOUT)
            {
                return E_PENDING;
            }
            else if (dwResult != WAIT_OBJECT_0)
            {
                return HRESULT_FROM_WIN32(GetLastError());
            }

            *pbEOS = m_bEOS;
            return m_hrStatus;
        }

    };

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
        ComPtr<IMFAttributes> srcReadAttrStore;
        hr = MFCreateAttributes(srcReadAttrStore.GetAddressOf(), 2);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create attributes store", "MFCreateAttributes");

        // enable DXVA encoding
        srcReadAttrStore->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, mfDXGIDevMan.Get());

        // enable codec hardware acceleration
        srcReadAttrStore->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto ucs2url = transcoder.from_bytes(url);

        hr = MFCreateSourceReaderFromURL(ucs2url.c_str(),
                                         srcReadAttrStore.Get(),
                                         m_mfSourceReader.GetAddressOf());

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create media source reader", "MFCreateSourceReaderFromURL");

        // Get the encoded video media type:
        ComPtr<IMFMediaType> srcEncVideoMType;
        hr = m_mfSourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, srcEncVideoMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to get video media type from source", "IMFSourceReader::GetNativeMediaType");

        auto uncompVideoMType = CreateUncompressedVideoMediaTypeFrom(srcEncVideoMType);

        // Set the source reader to use the new uncompressed video media type:
        hr = m_mfSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, uncompVideoMType.Get());
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set source reader output to uncompressed video media type",
                "IMFSourceReader::SetCurrentMediaType"
            );
        }

        // Get an alternative interface for source reader:
        ComPtr<IMFSourceReaderEx> sourceReaderAltIntf;
        hr = m_mfSourceReader.CopyTo(IID_PPV_ARGS(sourceReaderAltIntf.GetAddressOf()));
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to query for media source reader for alternative interface",
                "IMFSourceReader::CopyTo"
            );
        }

        // From the alternative interface, obtain the selected MFT for decoding:
        GUID transformCategory;
        ComPtr<IMFTransform> videoTransform;
        hr = sourceReaderAltIntf->GetTransformForStream(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            &transformCategory,
            videoTransform.GetAddressOf()
        );

        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get selected video MFT for source reader",
                "IMFSourceReaderEx::GetTransformForStream"
            );
        }

        // Get video MFT attributes store:
        ComPtr<IMFAttributes> mftAttrStore;
        hr = videoTransform->GetAttributes(mftAttrStore.GetAddressOf());
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get attributes of video MFT selected by source reader",
                "IMFAttributes::GetAttributes"
            );
        }

        // Will MFT use DXVA?
        std::cout << "Media source reader selected a video MFT "
                  << (MFGetAttributeUINT32(mftAttrStore.Get(), MF_SA_D3D_AWARE, FALSE) == TRUE
                      ? "that uses DXVA"
                      : "NOT accelerated by hardware") << std::endl;

        PWSTR mftFriendlyName;
        hr = MFGetAttributeString(mftAttrStore.Get(), MFT_FRIENDLY_NAME_Attribute, &mftFriendlyName);

        // Is MFT hardware based?
        if (SUCCEEDED(hr))
        {
            std::cout << "Selected MFT is hardware based: " << transcoder.to_bytes(mftFriendlyName) << std::endl;
            CoTaskMemFree(mftFriendlyName);
        }
        else
            std::cout << "Selected MFT is NOT hardware based" << std::endl;
        
        // Get the encoded audio media type:
        ComPtr<IMFMediaType> srcEncAudioMType;
        hr = m_mfSourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, srcEncAudioMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to get media type from source", "IMFSourceReader::GetNativeMediaType");

        // Set the source reader to use the new uncompressed audio media type, if not already uncompressed:

        auto uncompAudioMType = CreateUncompressedAudioMediaTypeFrom(srcEncAudioMType);

        if (uncompAudioMType)
        {
            hr = m_mfSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, uncompAudioMType.Get());
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
