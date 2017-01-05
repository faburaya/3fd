#include "stdafx.h"
#include "MediaFoundationWrappers.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include "3FD\utils.h"
#include <iostream>
#include <algorithm>
#include <codecvt>
#include <mfapi.h>
#include <codecapi.h>

namespace application
{
    using namespace _3fd::core;

    // Helps creating a video media type based in a model and parameters
    static ComPtr<IMFMediaType> CreateOutVideoMediaType(const DecodedMediaType &baseVideo,
                                                        double targeSizeFactor,
                                                        Encoder encoder)
    {
        CALL_STACK_TRACE;

        MFRatio pixelAspect;
        MFRatio videoFPS;
        UINT32  videoWidth;
        UINT32  videoHeigth;
        HRESULT hr;

        // From base media type, get some main attributes:
        if (FAILED(hr = MFGetAttributeSize(baseVideo.mediaType.Get(),
                                           MF_MT_FRAME_SIZE,
                                           &videoWidth,
                                           &videoHeigth)) ||

            FAILED(hr = MFGetAttributeRatio(baseVideo.mediaType.Get(),
                                            MF_MT_FRAME_RATE,
                                            reinterpret_cast<UINT32 *> (&videoFPS.Numerator),
                                            reinterpret_cast<UINT32 *> (&videoFPS.Denominator))) ||

            FAILED(hr = MFGetAttributeRatio(baseVideo.mediaType.Get(),
                                            MF_MT_PIXEL_ASPECT_RATIO,
                                            reinterpret_cast<UINT32 *> (&pixelAspect.Numerator),
                                            reinterpret_cast<UINT32 *> (&pixelAspect.Denominator))))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get attribute of media type for decoded video",
                "IMFMediaType::GetUINT32");
        }

        UINT32 videoAvgBitRate = static_cast<UINT32> (baseVideo.originalEncodedDataRate * targeSizeFactor);

        // Create the video output media type:
        ComPtr<IMFMediaType> outputVideoMType;
        hr = MFCreateMediaType(outputVideoMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create media type for video output", "MFCreateMediaType");

        // Now, in the new media type, set the base values plus some hardcoded ones:
        if (FAILED(hr = outputVideoMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) ||
            FAILED(hr = outputVideoMType->SetUINT32(MF_MT_AVG_BITRATE, videoAvgBitRate)) ||
            FAILED(hr = outputVideoMType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive)) ||

            FAILED(hr = MFSetAttributeSize(outputVideoMType.Get(),
                                           MF_MT_FRAME_SIZE,
                                           videoWidth,
                                           videoHeigth)) ||

            FAILED(hr = MFSetAttributeRatio(outputVideoMType.Get(),
                                            MF_MT_FRAME_RATE,
                                            videoFPS.Numerator,
                                            videoFPS.Denominator)) ||

            FAILED(hr = MFSetAttributeRatio(outputVideoMType.Get(),
                                            MF_MT_PIXEL_ASPECT_RATIO,
                                            pixelAspect.Numerator,
                                            pixelAspect.Denominator)))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set attribute of media type for video output",
                "IMFMediaType::SetUINT32 / SetGUID");
        }

        UINT32 profile;
        GUID videoFormat;

        switch (encoder)
        {
        case Encoder::H264_AVC:
            videoFormat = MFVideoFormat_H264;
            profile = eAVEncH264VProfile_High;
            break;
        case Encoder::H265_HEVC:
            videoFormat = MFVideoFormat_HEVC;
            profile = eAVEncH265VProfile_Main_420_8;
            break;
        default:
            _ASSERTE(false);
            break;
        }

        // Set video format (encoder) and profile:
        if (FAILED(hr = outputVideoMType->SetGUID(MF_MT_SUBTYPE, videoFormat)) ||
            FAILED(hr = outputVideoMType->SetUINT32(MF_MT_MPEG2_PROFILE, profile)))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set encoder/profile in media type for video output",
                "IMFMediaType::SetUINT32 / SetGUID");
        }

        return outputVideoMType;
    }

    // Helps creating an audio media type based in a model
    static ComPtr<IMFMediaType> CreateOutAudioMediaType(const DecodedMediaType &baseAudio)
    {
        CALL_STACK_TRACE;

        UINT32 numChannels;
        UINT32 sampleRate;
        UINT32 bitsPerSample;
        HRESULT hr;

        // From base media type, get some main attributes:
        if (FAILED(hr = baseAudio.mediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &numChannels)) ||
            FAILED(hr = baseAudio.mediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate)) ||
            FAILED(hr = baseAudio.mediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample)))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get attribute of media type for decoded audio",
                "IMFMediaType::GetUINT32");
        }

        // Find a lower data rate for the audio stream:

        UINT32 avgBytesPerSec(0UL);
        const UINT32 enumBpsVals[] = { 24000, 20000, 16000, 12000 };
        const auto endEnumBpsVals = enumBpsVals + (sizeof enumBpsVals) / sizeof(UINT32);
        auto iterEnumBps = std::upper_bound(enumBpsVals, endEnumBpsVals, baseAudio.originalEncodedDataRate);

        if (iterEnumBps != endEnumBpsVals)
            avgBytesPerSec = *iterEnumBps;

        // Create the audio output media type:
        ComPtr<IMFMediaType> outputAudioMType;
        hr = MFCreateMediaType(outputAudioMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create media type for audio output", "MFCreateMediaType");

        // Now set the values:

        if (FAILED(hr = outputAudioMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
            FAILED(hr = outputAudioMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC)) ||
            FAILED(hr = outputAudioMType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample)) ||
            FAILED(hr = outputAudioMType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate)) ||
            FAILED(hr = outputAudioMType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, numChannels)))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set attribute of media type for audio output",
                "IMFMediaType::SetUINT32 / SetGUID");
        }

        if (avgBytesPerSec != 0UL &&
            FAILED(hr = outputAudioMType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, avgBytesPerSec)))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set data rate of audio output",
                "IMFMediaType::SetUINT32");
        }

        return outputAudioMType;
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="MFSinkWriter"/> class.
    /// </summary>
    /// <param name="url">The URL.</param>
    /// <param name="mfDXGIDevMan">The mf dxgi dev man.</param>
    /// <param name="decodedMTypes">The video props.</param>
    /// <param name="videoFormat">The mf dxgi dev man.</param>
    /// <param name="targeSizeFactor">The mf dxgi dev man.</param>
    MFSinkWriter::MFSinkWriter(const string &url,
                               const ComPtr<IMFDXGIDeviceManager> &mfDXGIDevMan,
                               const std::map<DWORD, DecodedMediaType> &decodedMTypes,
                               double targeSizeFactor,
                               Encoder encoder)
    {
        CALL_STACK_TRACE;

        // Create attributes store, to set properties on the source reader:
        ComPtr<IMFAttributes> sinkReadAttrStore;
        HRESULT hr = MFCreateAttributes(sinkReadAttrStore.GetAddressOf(), 2);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create attributes store", "MFCreateAttributes");

        // enable DXVA encoding
        sinkReadAttrStore->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, mfDXGIDevMan.Get());

        // enable codec hardware acceleration
        sinkReadAttrStore->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto ucs2url = transcoder.from_bytes(url);

        // Create sink writer:
        hr = MFCreateSinkWriterFromURL(ucs2url.c_str(),
                                       nullptr,
                                       sinkReadAttrStore.Get(),
                                       m_mfSinkWriter.GetAddressOf());

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create sink writer", "MFCreateSinkWriterFromURL");


    }

}// end of namespace application
