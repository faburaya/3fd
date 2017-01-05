#include "stdafx.h"
#include "MediaFoundationWrappers.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include "3FD\utils.h"
#include <iostream>
#include <algorithm>
#include <codecvt>
#include <memory>

#include <Shlwapi.h>
#include <propvarutil.h>
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
            WWAPI::RaiseHResultException(hr, "Failed to create media type", "MFCreateMediaType");

        // Setup uncompressed media type mainly as a copy from original encoded version...
        hr = srcEncVideoMType->CopyAllItems(uncompVideoMType.Get());
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to copy attributes from original media type",
                "IMFMediaType::CopyAllItems");
        }

        // Set uncompressed video format:

        hr = uncompVideoMType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set video format YUY2 on uncompressed video media type",
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
            hr = MFSetAttributeRatio(uncompVideoMType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to set pixel aspect ratio of uncompressed video media type",
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
                "Failed to get audio subtype of original media type",
                "IMFMediaType::GetGUID");
        }

        ComPtr<IMFMediaType> uncompAudioMType;

        // Original subtype is already uncompressed:
        if (subType == MFAudioFormat_PCM)
            return uncompAudioMType;

        /* Get the sample rate and other information from the audio format.
           Note: Some encoded audio formats do not contain a value for bits/sample.
           In that case, use a default value of 16. Most codecs will accept this value. */

        auto channelMask = MFGetAttributeUINT32(srcEncAudioMType.Get(), MF_MT_AUDIO_CHANNEL_MASK, 0UL);
        auto numChannels = MFGetAttributeUINT32(srcEncAudioMType.Get(), MF_MT_AUDIO_NUM_CHANNELS, 0UL);
        auto sampleRate = MFGetAttributeUINT32(srcEncAudioMType.Get(), MF_MT_AUDIO_SAMPLES_PER_SECOND, 0UL);
        auto bitsPerSample = MFGetAttributeUINT32(srcEncAudioMType.Get(), MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

        if (numChannels == 0 || sampleRate == 0)
        {
            throw AppException<std::runtime_error>(
                "Could not retrieve information to create uncompressed media type "
                "for source, because it was not available in original media type"
            );
        }

        // Calculate derived values:
        auto blockAlign = numChannels * (bitsPerSample / 8);
        auto bytesPerSecond = blockAlign * sampleRate;

        // Create an empty media type:
        hr = MFCreateMediaType(uncompAudioMType.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create media type", "MFCreateMediaType");
        
        // Initialize attributes:

        if (FAILED(hr = uncompAudioMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
            FAILED(hr = uncompAudioMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bytesPerSecond)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, numChannels)) ||
            FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE)))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set attribute in uncompressed audio media type",
                "IMFMediaType::SetUINT32 / SetGUID");
        }

        // If channel mask was present in original stream, set it for the decoded stream too:
        if (channelMask != 0UL && FAILED(hr = uncompAudioMType->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, channelMask)))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to set channel mask in uncompressed audio media type",
                "IMFMediaType::SetUINT32");
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

        STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *) { return S_OK; }

        STDMETHODIMP OnFlush(DWORD) { return S_OK; }

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

    // Prints information about source reader selected MFT
    static void PrintTransformInfo(const ComPtr<IMFSourceReaderEx> &sourceReaderAltIntf, DWORD idxStream)
    {
        CALL_STACK_TRACE;

        std::cout << "\n========== source stream #" << idxStream << " ==========\n";

        HRESULT hr;
        GUID transformCategory;
        ComPtr<IMFTransform> videoTransform;
        DWORD idxMFT(0UL);

        // From the alternative interface, obtain the selected MFT for decoding:
        while (SUCCEEDED(hr = sourceReaderAltIntf->GetTransformForStream(idxStream,
                                                                         idxMFT,
                                                                         &transformCategory,
                                                                         videoTransform.ReleaseAndGetAddressOf())))
        {
            std::cout << "MFT " << idxMFT << ": " << TranslateMFTCategory(transformCategory);

            // Get video MFT attributes store:
            ComPtr<IMFAttributes> mftAttrStore;
            hr = videoTransform->GetAttributes(mftAttrStore.GetAddressOf());

            if (hr == E_NOTIMPL)
            {
                std::cout << std::endl;
                ++idxMFT;
                continue;
            }
            else if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to get attributes of MFT selected by source reader",
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
                "Failed to get selected MFT for source reader",
                "IMFSourceReaderEx::GetTransformForStream");
        }
    }

    /// <summary>
    /// Configures the video and audio decoders upon initialization or change of input media type.
    /// </summary>
    /// <param name="mustReconfigAll">if set to <c>true</c>, must reconfigure the decoders for all streams.</param>
    void MFSourceReader::ConfigureDecoderTransforms(bool mustReconfigAll)
    {
        CALL_STACK_TRACE;

        // Get an alternative interface for source reader:
        ComPtr<IMFSourceReaderEx> sourceReaderAltIntf;
        HRESULT hr = m_mfSourceReader.CopyTo(IID_PPV_ARGS(sourceReaderAltIntf.GetAddressOf()));
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to query source reader for alternative interface",
                "IMFSourceReader::CopyTo");
        }

        ComPtr<IMFMediaType> originalMType;
        DWORD idxStream(mustReconfigAll ? 0UL : m_streamCount);

        // Iterate over streams:
        while (SUCCEEDED(hr = m_mfSourceReader->GetNativeMediaType(idxStream, 0, originalMType.ReleaseAndGetAddressOf())))
        {
            // Get major type of input:
            GUID majorType = { 0 };
            hr = originalMType->GetMajorType(&majorType);
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to get major media type of original stream",
                    "IMFMediaType::GetMajorType");
            }

            // Video stream?
            if (majorType == MFMediaType_Video)
            {
                auto uncompVideoMType = CreateUncompressedVideoMediaTypeFrom(originalMType);

                // Set the source reader to use the new uncompressed video media type:
                HRESULT hr = m_mfSourceReader->SetCurrentMediaType(idxStream, nullptr, uncompVideoMType.Get());
                if (FAILED(hr))
                {
                    WWAPI::RaiseHResultException(hr,
                        "Failed to set decoded stream as uncompressed video type",
                        "IMFSourceReader::SetCurrentMediaType");
                }

                PrintTransformInfo(sourceReaderAltIntf, idxStream);
            }
            // Audio stream?
            else if (majorType == MFMediaType_Audio)
            {
                auto uncompAudioMType = CreateUncompressedAudioMediaTypeFrom(originalMType);

                // Set the source reader to use the new uncompressed audio media type, if not already uncompressed:
                if (uncompAudioMType)
                {
                    hr = m_mfSourceReader->SetCurrentMediaType(idxStream, nullptr, uncompAudioMType.Get());
                    if (FAILED(hr))
                    {
                        WWAPI::RaiseHResultException(hr,
                            "Failed to set decoded stream as uncompressed audio type",
                            "IMFSourceReader::SetCurrentMediaType");
                    }
                }

                PrintTransformInfo(sourceReaderAltIntf, idxStream);
            }
            // Unknown type of stream:
            else
            {
                m_mfSourceReader->SetStreamSelection(idxStream, FALSE);
                if (FAILED(hr))
                {
                    WWAPI::RaiseHResultException(hr,
                        "Failed to unselect stream for reading",
                        "IMFSourceReader::SetStreamSelection");
                }
            }

            m_streamCount = idxStream++;
        }// while loop end

        if (hr != MF_E_INVALIDSTREAMNUMBER)
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get media type of original stream",
                "IMFSourceReader::GetNativeMediaType");
        }
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="MFSourceReader"/> class.
    /// </summary>
    /// <param name="url">The URL of the media source.</param>
    /// <param name="mfDXGIDevMan">Microsoft DXGI device manager reference.</param>
    MFSourceReader::MFSourceReader(const string &url, const ComPtr<IMFDXGIDeviceManager> &mfDXGIDevMan)
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

        // enable DXVA decoding
        srcReadAttrStore->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, mfDXGIDevMan.Get());

        // enable codec hardware acceleration
        srcReadAttrStore->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

        // register a callback for asynchronous reading of samples
        srcReadAttrStore->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, m_srcReadCallback.Get());

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto ucs2url = transcoder.from_bytes(url);

        // create source reader:
        hr = MFCreateSourceReaderFromURL(ucs2url.c_str(),
                                         srcReadAttrStore.Get(),
                                         m_mfSourceReader.GetAddressOf());

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create source reader", "MFCreateSourceReaderFromURL");

        // select all streams for reading:
        hr = m_mfSourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, TRUE);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to select streams for reading", "IMFSourceReader::SetStreamSelection");

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
    /// Gets the media types output by streams of a given range.
    /// </summary>
    /// <param name="idxStream">Index of the first stream whose media type will be retrieved.</param>
    /// <param name="outMediaTypes">Will receive a dictionary of mediay types indexed by stream index.</param>
    /// <param name="duration">Will be set with the duration of the media file.</param>
    void MFSourceReader::GetOutputMediaTypesFrom(DWORD idxStream,
                                                 std::map<DWORD, DecodedMediaType> &decodedMTypes,
                                                 std::chrono::microseconds &duration) const
    {
        CALL_STACK_TRACE;

        // First get the duration of the media file:

        PROPVARIANT variant;
        HRESULT hr = m_mfSourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &variant);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get duration of source media file",
                "IMFSourceReader::GetPresentationAttribute");
        }

        ULONGLONG durationIn100ns;
        hr = PropVariantToUInt64(variant, &durationIn100ns);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to cast value out of variant type", "PropVariantToUInt64");

        duration = std::chrono::microseconds(durationIn100ns / 10);

        std::map<DWORD, DecodedMediaType> decMTypesByIndex;
        ComPtr<IMFMediaType> decodedMType;

        // Iterate over streams:
        while (SUCCEEDED(hr = m_mfSourceReader->GetCurrentMediaType(idxStream, decodedMType.ReleaseAndGetAddressOf())))
        {
            BOOL selected;
            hr = m_mfSourceReader->GetStreamSelection(idxStream, &selected);
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to determine selection of source reader stream",
                    "IMFSourceReader::GetStreamSelection");
            }

            if (!selected)
            {
                ++idxStream;
                continue;
            }

            DecodedMediaType decoded;
            decoded.mediaType = decodedMType;

            // Also get the media type in the original stream...
            ComPtr<IMFMediaType> originalMType;
            hr = m_mfSourceReader->GetNativeMediaType(idxStream, 0, originalMType.GetAddressOf());
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to get media type of original stream",
                    "IMFSourceReader::GetNativeMediaType");
            }

            // and its major type...
            GUID majorType = { 0 };
            hr = originalMType->GetMajorType(&majorType);
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to get major media type of original stream",
                    "IMFMediaType::GetMajorType");
            }

            // ... just to know its original bitrate
            hr = originalMType->GetUINT32(
                (majorType == MFMediaType_Video ? MF_MT_AVG_BITRATE : MF_MT_AUDIO_AVG_BYTES_PER_SECOND),
                &decoded.originalEncodedDataRate
            );

            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to get data rate of original stream",
                    "IMFMediaType::GetUINT32");
            }

            auto insertResult = decMTypesByIndex.emplace(idxStream++, decoded);
            _ASSERTE(insertResult.second); // succesfully inserted?

        }// while loop end

        if (hr != MF_E_INVALIDSTREAMNUMBER)
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to get media type of decoded stream",
                "IMFSourceReader::GetCurrentMediaType");
        }

        decodedMTypes.swap(decMTypesByIndex);
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
    /// Gets the read sample, after an asynchronous call had been issued.
    /// </summary>
    /// <param name="event">Receives what event might have come on the stream during the read operation.</param>
    /// <returns>The read sample. May be null depending on the reported stream event.</returns>
    ComPtr<IMFSample> MFSourceReader::GetSample(DWORD &state)
    {
        CALL_STACK_TRACE;

        auto result = static_cast<MFSourceReaderCallbackImpl *> (m_srcReadCallback.Get())->GetResult();

        // error?
        if ((result->streamFlags & MF_SOURCE_READERF_ERROR) != 0)
            WWAPI::RaiseHResultException(result->hres, "Source reader failed to read sample", "IMFSourceReader::ReadSample");

        state = result->streamFlags;

        // change of current media type is not expected
        _ASSERTE((result->streamFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) == 0);

        // new stream available:
        if ((state & MF_SOURCE_READERF_NEWSTREAM) != 0)
        {
            // select all streams:
            HRESULT hr = m_mfSourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, TRUE);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to select streams for reading", "IMFSourceReader::SetStreamSelection");

            // reconfigure decoders (all or just the new)
            ConfigureDecoderTransforms((result->streamFlags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED) != 0);

            std::cout << "\nNew stream is available in source reader!\n" << std::endl;
        }
        // change of native media type in one or more streams:
        else if ((result->streamFlags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED) != 0)
        {
            ConfigureDecoderTransforms(true);
            std::cout << "\nNative media type has changed in source stream!\n" << std::endl;
        }

        return result->sample;
    }

}// end of namespace application
