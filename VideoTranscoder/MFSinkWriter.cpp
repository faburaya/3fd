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
#include <Mferror.h>
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

    // Prints information about sink writer selected MFT
    static void PrintTransformInfo(const ComPtr<IMFSinkWriterEx> &sinkWriterAltIntf, DWORD idxStream)
    {
        CALL_STACK_TRACE;

        std::cout << "\n=========== sink stream #" << idxStream << " ===========\n";

        HRESULT hr;
        GUID transformCategory;
        ComPtr<IMFTransform> transform;
        DWORD idxMFT(0UL);

        // From the alternative interface, obtain the selected MFT for decoding:
        while (SUCCEEDED(hr = sinkWriterAltIntf->GetTransformForStream(idxStream,
                                                                       idxMFT,
                                                                       &transformCategory,
                                                                       transform.ReleaseAndGetAddressOf())))
        {
            std::cout << "MFT " << idxMFT << ": " << TranslateMFTCategory(transformCategory);

            // Get video MFT attributes store:
            ComPtr<IMFAttributes> mftAttrStore;
            hr = transform->GetAttributes(mftAttrStore.GetAddressOf());

            if (hr == E_NOTIMPL)
            {
                std::cout << std::endl;
                ++idxMFT;
                continue;
            }
            else if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to get attributes of MFT selected by sink writer",
                    "IMFTransform::GetAttributes");
            }

            // Will MFT use DXVA?
            if (MFGetAttributeUINT32(mftAttrStore.Get(), MF_SA_D3D_AWARE, FALSE) == TRUE)
                std::cout << ", supports DXVA";

            PWSTR mftFriendlyName;
            hr = MFGetAttributeString(mftAttrStore.Get(), MFT_FRIENDLY_NAME_Attribute, &mftFriendlyName);

            // Is MFT hardware based?
            if (SUCCEEDED(hr))
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
                std::cout << ", hardware based (" << transcoder.to_bytes(mftFriendlyName) << ')';
                CoTaskMemFree(mftFriendlyName);
            }

            std::cout << std::endl;
            ++idxMFT;
        }

        if (hr != MF_E_INVALIDINDEX)
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get selected MFT for sink writer",
                "IMFSinkWriterEx::GetTransformForStream");
        }
    }

    /// <summary>
    /// Adds a new (decoded) stream (input coming from source reader, found there)
    /// to the sink writer, hence creating a new (encoded) output stream.
    /// </summary>
    /// <param name="sinkWriterAltIntf">An alternative interface to the sink writer,
    /// capable of retrieving the selected media transform for each stream.</param>
    /// <param name="idxDecStream">The index of the stream in source.</param>
    /// <param name="decoded">Describes the decoded stream to add.</param>
    /// <param name="targeSizeFactor">The target size of the video output, as a fraction of the originally encoded version.</param>
    /// <param name="encoder">What encoder to use for video.</param>
    void MFSinkWriter::AddStream(const ComPtr<IMFSinkWriterEx> &sinkWriterAltIntf,
                                 DWORD idxDecStream,
                                 const DecodedMediaType &decoded,
                                 double targeSizeFactor,
                                 Encoder encoder)
    {
        CALL_STACK_TRACE;

        // Get the major type of the decoded input stream:
        GUID majorType = { 0 };
        HRESULT hr = decoded.mediaType->GetMajorType(&majorType);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get major media type of decoded stream",
                "IMFMediaType::GetMajorType");
        }

        ComPtr<IMFMediaType> outputMType;
        MediaDataType mediaDataType;

        if (majorType == MFMediaType_Video) // video? will encode
        {
            mediaDataType = Video;
            outputMType = CreateOutVideoMediaType(decoded, targeSizeFactor, encoder);
        }
        else if (majorType == MFMediaType_Audio) // audio? will encode
        {
            mediaDataType = Audio;
            outputMType = CreateOutAudioMediaType(decoded);
        }
        else // other? will copy
            outputMType = decoded.mediaType;

        // Add the output stream:
        DWORD idxOutStream;
        hr = m_mfSinkWriter->AddStream(outputMType.Get(), &idxOutStream);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to add output stream to media sink writer",
                "IMFSinkWriter::AddStream");
        }

        // Set the decoded input too:
        hr = m_mfSinkWriter->SetInputMediaType(idxOutStream, decoded.mediaType.Get(), nullptr);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set decoded media type as sink writer input stream",
                "IMFSinkWriter::SetInputMediaType");
        }

        PrintTransformInfo(sinkWriterAltIntf, idxOutStream);

        /* The stream index in sink and source can be different, hence it must be
        kept in a lookup table for fast conversion during encoding process */
        m_streamInfoLookupTab[idxDecStream] = StreamInfo{ static_cast<uint16_t> (idxOutStream), mediaDataType };
    }

    /// <summary>
    /// Adds new (decoded) streams (input coming from source reader, found there)
    /// to the sink writer, hence creating new (encoded) output streams.
    /// </summary>
    /// <param name="decodedMTypes">A dictionary of (decoded input) media types indexed by stream index.</param>
    /// <param name="targeSizeFactor">The target size of the video output, as a fraction of the originally encoded version.</param>
    /// <param name="encoder">What encoder to use for video.</param>
    void MFSinkWriter::AddNewStreams(const std::map<DWORD, DecodedMediaType> &decodedMTypes,
                                     double targeSizeFactor,
                                     Encoder encoder)
    {
        CALL_STACK_TRACE;

        // Prepare the lookup table for index conversion (source reader index -> sink writer index):
        auto prevSize = m_streamInfoLookupTab.size();
        _ASSERTE(!decodedMTypes.empty() && decodedMTypes.begin()->first >= prevSize);
        m_streamInfoLookupTab.resize(decodedMTypes.rbegin()->first + 1);
        std::fill(m_streamInfoLookupTab.begin() + prevSize, m_streamInfoLookupTab.end(), StreamInfo{});

        // Get an alternative interface for sink writer:
        ComPtr<IMFSinkWriterEx> sinkWriterAltIntf;
        HRESULT hr = m_mfSinkWriter.CopyTo(IID_PPV_ARGS(sinkWriterAltIntf.GetAddressOf()));
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to query sink writer for alternative interface",
                "IMFSinkWriterEx::CopyTo");
        }

        // Iterate over decoded input streams:
        for (auto &entry : decodedMTypes)
        {
            auto idxDecStream = entry.first;
            auto &decoded = entry.second;

            AddStream(sinkWriterAltIntf,
                     idxDecStream,
                     decoded,
                     targeSizeFactor,
                     encoder);
        }

        if (m_streamInfoLookupTab.empty())
            return;

        // Prepare store for tracking of gap occurences in streams:
        prevSize = m_streamsGapsTracking.size();
        m_streamsGapsTracking.resize(m_streamInfoLookupTab.back().outIndex + 1);
        std::fill(m_streamsGapsTracking.begin() + prevSize, m_streamsGapsTracking.end(), -1LL);
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="MFSinkWriter"/> class.
    /// </summary>
    /// <param name="url">The URL of the media source.</param>
    /// <param name="mfDXGIDevMan">Microsoft DXGI device manager reference.</param>
    /// <param name="decodedMTypes">A dictionary of (decoded input) media types indexed by stream index.</param>
    /// <param name="targeSizeFactor">The target size of the video output, as a fraction of the originally encoded version.</param>
    /// <param name="encoder">What encoder to use for video.</param>
    MFSinkWriter::MFSinkWriter(const string &url,
                               const ComPtr<IMFDXGIDeviceManager> &mfDXGIDevMan,
                               const std::map<DWORD, DecodedMediaType> &decodedMTypes,
                               double targeSizeFactor,
                               Encoder encoder)
    try
    {
        CALL_STACK_TRACE;

        // Create attributes store, to set properties on the sink writer:
        ComPtr<IMFAttributes> sinkWriterAttrStore;
        HRESULT hr = MFCreateAttributes(sinkWriterAttrStore.GetAddressOf(), 2);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create attributes store", "MFCreateAttributes");

        // enable DXVA encoding
        sinkWriterAttrStore->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, mfDXGIDevMan.Get());

        // enable codec hardware acceleration
        sinkWriterAttrStore->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto ucs2url = transcoder.from_bytes(url);

        // Create sink writer:
        hr = MFCreateSinkWriterFromURL(ucs2url.c_str(),
                                       nullptr,
                                       sinkWriterAttrStore.Get(),
                                       m_mfSinkWriter.GetAddressOf());

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create sink writer", "MFCreateSinkWriterFromURL");

        if (decodedMTypes.empty())
            return;

        AddNewStreams(decodedMTypes, targeSizeFactor, encoder);

        // Start async encoding (will await for samples)
        hr = m_mfSinkWriter->BeginWriting();
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to start asynchronous encoding", "IMFSinkWriter::BeginWriting");
    }
    catch (IAppException &ex)
    {
        CALL_STACK_TRACE;
        throw AppException<std::runtime_error>("Failed to create sink writer", ex);
    }
    catch (std::exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Generic failure when creating sink writer: " << ex.what();
        throw AppException<std::runtime_error>(oss.str());
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="MFSinkWriter"/> class.
    /// </summary>
    MFSinkWriter::~MFSinkWriter()
    {
        CALL_STACK_TRACE;

        HRESULT hr = m_mfSinkWriter->Finalize();
        if (FAILED(hr))
            Logger::Write(hr, "Failed to flush and finalize media sink", "IMFSinkWriter::Finalize", Logger::PRIO_CRITICAL);
    }

    /// <summary>
    /// Encodes a sample. (This is a blocking call.)
    /// </summary>
    /// <param name="idxDecStream">The index of the decoded stream where the sample came from.</param>
    /// <param name="sample">The sample to encode.</param>
    void MFSinkWriter::EncodeSample(DWORD idxDecStream, const ComPtr<IMFSample> &sample)
    {
        CALL_STACK_TRACE;

        auto &streamInfo = m_streamInfoLookupTab[idxDecStream];
        auto &lastGap = m_streamsGapsTracking[streamInfo.outIndex];

        // was in a gap until last call?
        if (lastGap > 0)
        {
            lastGap = -1; // means "out of gap"
            sample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
        }

        // enqueue the sample for asynchronous encoding:
        HRESULT hr = m_mfSinkWriter->WriteSample(streamInfo.outIndex, sample.Get());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to put sample in encoding queue", "IMFSinkWriter::WriteSample");
    }

    /// <summary>
    /// Places a gap in the given stream and timestamp.
    /// </summary>
    /// <param name="idxDecStream">The index of the decoded stream
    /// (in source reader) where the gap was spotted.</param>
    /// <param name="timestamp">The timestamp.</param>
    void MFSinkWriter::PlaceGap(DWORD idxDecStream, LONGLONG timestamp)
    {
        CALL_STACK_TRACE;

        auto &streamInfo = m_streamInfoLookupTab[idxDecStream];
        
        /* "For video, call this method once for each missing frame.
            For audio, call this method at least once per second during a gap in the audio."
            https://msdn.microsoft.com/en-us/library/windows/desktop/dd374652.aspx */
        
        if (streamInfo.mediaDType == Video)
        {
            HRESULT hr = m_mfSinkWriter->SendStreamTick(streamInfo.outIndex, timestamp);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to send video stream tick to encoder", "IMFSinkWriter::SendStreamTick");

            m_streamsGapsTracking[streamInfo.outIndex] = timestamp; // remember last gap instant
        }
        else if (streamInfo.mediaDType == Audio)
        {
            auto &lastGap = m_streamsGapsTracking[streamInfo.outIndex];

            if (lastGap < 0 || timestamp - lastGap > 1e7 /* x 100 ns */)
            {
                lastGap = timestamp; // remember last tick sent

                HRESULT hr = m_mfSinkWriter->SendStreamTick(streamInfo.outIndex, timestamp);
                if (FAILED(hr))
                    WWAPI::RaiseHResultException(hr, "Failed to send audio stream tick to encoder", "IMFSinkWriter::SendStreamTick");
            }
        }
        else
            _ASSERTE(false); // unexpected media data type
    }

}// end of namespace application
