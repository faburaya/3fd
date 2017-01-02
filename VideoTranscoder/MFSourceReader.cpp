#include "stdafx.h"
#include "MediaFoundationWrappers.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"

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
                Logger::PRIO_CRITICAL
            );
        }
    }

    //////////////////////////////
    // Class MFSourceReader
    //////////////////////////////

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
                "Failed to set video format RGB32 on uncompressed source media type",
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
                    "Failed to set pixel aspect ratio of uncompressed source media type",
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

    }

    /// <summary>
    /// Initializes a new instance of the <see cref="MFSourceReader"/> class.
    /// </summary>
    /// <param name="url">The URL.</param>
    MFSourceReader::MFSourceReader(const string &url)
    {
        CALL_STACK_TRACE;

        HRESULT hr = MFCreateAttributes(m_mfAttrStore.GetAddressOf(), 1);

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create attributes store", "MFCreateAttributes");

        // enable codec hardware acceleration
        m_mfAttrStore->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto ucs2url = transcoder.from_bytes(url);

        hr = MFCreateSourceReaderFromURL(ucs2url.c_str(),
                                         m_mfAttrStore.Get(),
                                         m_mfSourceReader.GetAddressOf());

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create media source reader", "MFCreateSourceReaderFromURL");

        // Get the encoded media type:
        ComPtr<IMFMediaType> mfSrcEncVideoMType;
        hr = m_mfSourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, mfSrcEncVideoMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to get media type from source", "IMFSourceReader::GetNativeMediaType");

        // Set the source reader to use the new uncompressed media type:
        auto mfUncompVideoMType = CreateUncompressedVideoMediaType(mfSrcEncVideoMType);

        if (mfUncompVideoMType)
        {
            hr = m_mfSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, mfUncompVideoMType.Get());
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to set source reader output to uncompressed media type",
                    "IMFSourceReader::SetCurrentMediaType"
                );
            }
        }
        

    }

}// end of namespace application
