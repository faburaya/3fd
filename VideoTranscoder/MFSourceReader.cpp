#include "stdafx.h"
#include "MediaFoundationWrappers.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include "3FD\utils.h"
#include <iostream>
#include <algorithm>
#include <codecvt>
#include <atomic>
#include <memory>

#include <Shlwapi.h>
#include <mfapi.h>
#include <Mferror.h>

namespace application
{
    using namespace _3fd::core;

    // Helps creating an uncompressed video media type (based on original) for source reader
    static ComPtr<IMFMediaType> CreateUncompressedVideoMediaTypeFrom(const ComPtr<IMFMediaType> &srcEncVideoMType)
    {
        CALL_STACK_TRACE;
        
        // Create the uncompressed media type:
        ComPtr<IMFMediaType> uncompVideoMType;
        HRESULT hr = MFCreateMediaType(uncompVideoMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create uncompressed media type for source", "MFCreateMediaType");

        // Setup uncompressed media type mainly as a copy from original encoded version...
        hr = srcEncVideoMType->CopyAllItems(uncompVideoMType.Get());
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to copy attributes from original source media type",
                "IMFMediaType::CopyAllItems");
        }

        // Set uncompressed video format:

        hr = uncompVideoMType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set video format YUY2 on uncompressed media type for source",
                "IMFMediaType::SetGUID");
        }

        hr = uncompVideoMType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set video media type as uncompressed",
                "IMFMediaType::SetUINT32");
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
                    "IMFMediaType::SetGUID");
            }
        }

        return uncompVideoMType;
    }

    // Helps creating an uncompressed audio media type for source reader
    static ComPtr<IMFMediaType> CreateUncompressedAudioMediaTypeFrom(const ComPtr<IMFMediaType> &srcEncAudioMType)
    {
        CALL_STACK_TRACE;

        // Get subtype of input:
        GUID subType = { 0 };
        HRESULT hr = srcEncAudioMType->GetGUID(MF_MT_SUBTYPE, &subType);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to retrieve audio subtype from original source media type",
                "IMFMediaType::GetGUID");
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
                "IMFMediaType::SetGUID / SetUINT32");
        }

        return uncompAudioMType;
    }

    ///////////////////////////////////////////
    // Source Reader Callback Implementation
    ///////////////////////////////////////////

    /// <summary>
    /// Implementation of callback utilized by <see cref="MFSourceReader"/>.
    /// </summary>
    /// <seealso cref="IMFSourceReaderCallback" />
    class MFSourceReaderCallbackImpl : public IMFSourceReaderCallback
    {
    private:

        std::atomic<ULONG> m_referenceCount;
        _3fd::utils::Event m_resAvailableEvent;

        struct ReadResult
        {
            HRESULT hres;
            DWORD streamIndex;
            DWORD streamFlags;
            LONGLONG sampleTimestamp;
            ComPtr<IMFSample> sample;

            ReadResult(HRESULT pHRes,
                       DWORD pStreamIndex,
                       DWORD pStreamFlags,
                       LONGLONG pSampleTimestamp,
                       IMFSample *pSample) :
                hres(pHRes),
                streamIndex(pStreamIndex),
                streamFlags(pStreamFlags),
                sampleTimestamp(pSampleTimestamp),
                sample(pSample)
            {}
        };

        std::unique_ptr<ReadResult> m_result;

        /// <summary>
        /// Finalizes an instance of the <see cref="MFSourceReaderCallbackImpl"/> class.
        /// </summary>
        /// <remarks>Destructor is private. Caller should call <see cref="Release"/>.</remarks>
        virtual ~MFSourceReaderCallbackImpl() {}

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="MFSourceReaderCallbackImpl"/> class.
        /// </summary>
        MFSourceReaderCallbackImpl() :
            m_referenceCount(1UL) {}

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
            return m_referenceCount.fetch_add(1UL, std::memory_order_acq_rel);
        }

        STDMETHODIMP_(ULONG) Release()
        {
            ULONG currentCount = m_referenceCount.fetch_sub(1UL, std::memory_order_acq_rel) - 1UL;
            if (currentCount == 0)
                delete this;

            return currentCount;
        }

        STDMETHODIMP OnReadSample(HRESULT hr,
                                  DWORD streamIndex,
                                  DWORD streamFlags,
                                  LONGLONG timestamp,
                                  IMFSample *sample)
        {
            try
            {
                m_result.reset(
                    new ReadResult(hr, streamIndex, streamFlags, timestamp, sample)
                );

                m_resAvailableEvent.Signalize();
                return S_OK;
            }
            catch (IAppException &ex)
            {
                Logger::Write(ex, Logger::PRIO_CRITICAL);
                return E_FAIL;
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when retrieving sample read by media source reader: " << ex.what();
                Logger::Write(oss.str(), Logger::PRIO_CRITICAL);
                return E_FAIL;
            }
        }

        STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *)
        {
            return S_OK;
        }

        STDMETHODIMP OnFlush(DWORD)
        {
            return S_OK;
        }

    public:

        /// <summary>
        /// Gets the result of the asynchronous call.
        /// </summary>
        /// <returns>An object holding all the values returned by the source reader.</returns>
        std::unique_ptr<ReadResult> GetResult()
        {
            m_resAvailableEvent.Wait(
                [this]() { return m_result.get() != nullptr; }
            );

            return std::move(m_result);
        }
    };

    // Prints information about source reader selected video MFT
    static void PrintVideoTransformInfo(const ComPtr<IMFSourceReaderEx> &sourceReaderAltIntf)
    {
        CALL_STACK_TRACE;

        // From the alternative interface, obtain the selected MFT for decoding:
        GUID transformCategory;
        ComPtr<IMFTransform> videoTransform;
        HRESULT hr = sourceReaderAltIntf->GetTransformForStream(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            &transformCategory,
            videoTransform.GetAddressOf()
        );

        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get selected video MFT for source reader",
                "IMFSourceReaderEx::GetTransformForStream");
        }

        // Get video MFT attributes store:
        ComPtr<IMFAttributes> mftAttrStore;
        hr = videoTransform->GetAttributes(mftAttrStore.GetAddressOf());
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get attributes of video MFT selected by source reader",
                "IMFAttributes::GetAttributes");
        }

        // Will MFT use DXVA?
        std::cout << "Media source reader selected a video MFT "
            << (MFGetAttributeUINT32(mftAttrStore.Get(), MF_SA_D3D_AWARE, FALSE) == TRUE
                ? "that supports DXVA"
                : "does NOT support DXVA") << std::endl;

        PWSTR mftFriendlyName;
        hr = MFGetAttributeString(mftAttrStore.Get(), MFT_FRIENDLY_NAME_Attribute, &mftFriendlyName);

        // Is MFT hardware based?
        if (SUCCEEDED(hr))
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            std::cout << "Selected MFT is hardware based: " << transcoder.to_bytes(mftFriendlyName) << std::endl;
            CoTaskMemFree(mftFriendlyName);
        }
        else
            std::cout << "Selected MFT is NOT hardware based" << std::endl;
    }

    /// <summary>
    /// Configures the video and audio decoders upon initialization or change of input media type.
    /// </summary>
    /// <param name="mustReconfigAll">if set to <c>true</c>, must reconfigure the decoders for all streams.</param>
    void MFSourceReader::ConfigureDecoderTransforms(bool mustReconfigAll)
    {
        CALL_STACK_TRACE;

        ComPtr<IMFMediaType> srcEncodedMType;
        HRESULT hr(S_OK);
        DWORD idxStream(mustReconfigAll ? 0UL : m_streamCount);

        // Iterate over streams:
        while (SUCCEEDED(hr = m_mfSourceReader->GetNativeMediaType(idxStream, 0, srcEncodedMType.GetAddressOf())))
        {
            // Get major type of input:
            GUID majorType = { 0 };
            hr = srcEncodedMType->GetMajorType(&majorType);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to retrieve major media type", "IMFMediaType::GetMajorType");

            // Video stream?
            if (majorType == MFMediaType_Video)
            {
                auto uncompVideoMType = CreateUncompressedVideoMediaTypeFrom(srcEncodedMType);

                // Set the source reader to use the new uncompressed video media type:
                HRESULT hr = m_mfSourceReader->SetCurrentMediaType(idxStream, nullptr, uncompVideoMType.Get());
                if (FAILED(hr))
                {
                    WWAPI::RaiseHResultException(hr,
                        "Failed to set source reader output to uncompressed video media type",
                        "IMFSourceReader::SetCurrentMediaType");
                }

                // Get an alternative interface for source reader:
                ComPtr<IMFSourceReaderEx> sourceReaderAltIntf;
                hr = m_mfSourceReader.CopyTo(IID_PPV_ARGS(sourceReaderAltIntf.GetAddressOf()));
                if (FAILED(hr))
                {
                    WWAPI::RaiseHResultException(hr,
                        "Failed to query source reader for alternative interface",
                        "IMFSourceReader::CopyTo");
                }

                PrintVideoTransformInfo(sourceReaderAltIntf);
            }
            // Audio stream?
            else if (majorType == MFMediaType_Audio)
            {
                auto uncompAudioMType = CreateUncompressedAudioMediaTypeFrom(srcEncodedMType);

                // Set the source reader to use the new uncompressed audio media type, if not already uncompressed:
                if (uncompAudioMType)
                {
                    hr = m_mfSourceReader->SetCurrentMediaType(idxStream, nullptr, uncompAudioMType.Get());
                    if (FAILED(hr))
                    {
                        WWAPI::RaiseHResultException(hr,
                            "Failed to set source reader output to uncompressed audio media type",
                            "IMFSourceReader::SetCurrentMediaType");
                    }
                }
            }

            ++idxStream;
        }// while loop end

        if (hr != MF_E_INVALIDSTREAMNUMBER)
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get video media type from source",
                "IMFSourceReader::GetNativeMediaType");
        }

        m_streamCount = idxStream;
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="MFSourceReader"/> class.
    /// </summary>
    /// <param name="url">The URL of the media source.</param>
    /// <param name="mfDXGIDevMan">Microsoft DXGI device manager reference.</param>
    MFSourceReader::MFSourceReader(const string &url, ComPtr<IMFDXGIDeviceManager> &mfDXGIDevMan)
    try :
        m_streamCount(0UL),
        m_srcReadCallback(new MFSourceReaderCallbackImpl())
    {
        CALL_STACK_TRACE;

        // Create attributes store, to set properties on the source reader:
        ComPtr<IMFAttributes> srcReadAttrStore;
        HRESULT hr = MFCreateAttributes(srcReadAttrStore.GetAddressOf(), 3);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create attributes store", "MFCreateAttributes");

        // enable DXVA encoding
        srcReadAttrStore->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, mfDXGIDevMan.Get());

        // enable codec hardware acceleration
        srcReadAttrStore->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

        // register a callback for asynchronous reading of samples
        srcReadAttrStore->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, m_srcReadCallback.Get());

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto ucs2url = transcoder.from_bytes(url);

        hr = MFCreateSourceReaderFromURL(ucs2url.c_str(),
                                         srcReadAttrStore.Get(),
                                         m_mfSourceReader.GetAddressOf());

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create source reader", "MFCreateSourceReaderFromURL");

        ConfigureDecoderTransforms(true);
    }
    catch (IAppException &ex)
    {
        throw AppException<std::runtime_error>("Failed to create source reader", ex);
    }
    catch (std::exception &ex)
    {
        std::ostringstream oss;
        oss << "Generic failure when creating source reader: " << ex.what();
        throw AppException<std::runtime_error>(oss.str());
    }

    /// <summary>
    /// Reads a sample from source asynchronously.
    /// </summary>
    void MFSourceReader::ReadSampleAsync()
    {
        CALL_STACK_TRACE;

        HRESULT hr = m_mfSourceReader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, nullptr, nullptr, nullptr, nullptr);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Source reader failed to request asynchronous read of sample",
                "IMFSourceReader::ReadSample");
        }
    }

    /// <summary>
    /// Gets the sample.
    /// </summary>
    /// <returns></returns>
    ComPtr<IMFSample> MFSourceReader::GetSample()
    {
        CALL_STACK_TRACE;

        HRESULT hr;

        auto result = m_srcReadCallback->GetResult();

        // error?
        if ((result->streamFlags & MF_SOURCE_READERF_ERROR) != 0)
            WWAPI::RaiseHResultException(result->hres, "Source reader failed to read sample", "IMFSourceReader::ReadSample");

        // new stream available:
        if (result->streamFlags & MF_SOURCE_READERF_NEWSTREAM)
        {
            // Select all streams:
            hr = m_mfSourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, TRUE);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to select streams for reading", "IMFSourceReader::SetStreamSelection");

            ConfigureDecoderTransforms((result->streamFlags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED) != 0);
        }
        // change of native media type in one or more streams:
        else if ((result->streamFlags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED) != 0)
            ConfigureDecoderTransforms(true);

        if ((result->streamFlags & MF_SOURCE_READERF_STREAMTICK) != 0)
            ;

        if ((result->streamFlags & MF_SOURCE_READERF_ALLEFFECTSREMOVED) != 0)
            ;

        if ((result->streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
            ;

        if (result)
            return result->sample;

        return ComPtr<IMFSample>();
    }

}// end of namespace application
