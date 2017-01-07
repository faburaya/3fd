#ifndef MEDIAFOUNDATIONWRAPPERS_H // header guard
#define MEDIAFOUNDATIONWRAPPERS_H

#include "3FD\base.h"
#include <string>
#include <chrono>
#include <vector>
#include <map>
#include <wrl.h>
#include <d3d11.h>
#include <mfreadwrite.h>

namespace application
{
    using std::string;

    using namespace Microsoft::WRL;

    /// <summary>
    /// Uses RAII to initialize and finalize Microsoft Media Foundation library.
    /// </summary>
    class MediaFoundationLib
    {
    public:

        MediaFoundationLib();
        ~MediaFoundationLib();
    };

    const char *TranslateMFTCategory(const GUID &mftCategory);

    ComPtr<ID3D11Device> GetDeviceDirect3D(UINT idxVideoAdapter);

    /// <summary>
    /// Packs the necessary stream metadata from source reader
    /// that needs to be known for setup of the sink writer.
    /// </summary>
    struct DecodedMediaType
    {
        UINT32 originalEncodedDataRate;
        ComPtr<IMFMediaType> mediaType;
    };

    /// <summary>
    /// Enumerates the possible states of a stream being read, that would
    /// require some action in the application loop for transcoding.
    /// </summary>
    enum class ReadStateFlags : DWORD
    {
        EndOfStream = MF_SOURCE_READERF_ENDOFSTREAM,
        NewStreamAvailable = MF_SOURCE_READERF_NEWSTREAM,
        GapFound = MF_SOURCE_READERF_STREAMTICK
    };

    /// <summary>
    /// Wraps Media Foundation Source Reader object.
    /// </summary>
    class MFSourceReader : notcopiable
    {
    private:

        DWORD m_streamCount;
        ComPtr<IMFSourceReader> m_mfSourceReader;
        ComPtr<IMFSourceReaderCallback> m_srcReadCallback;

        void ConfigureDecoderTransforms(bool mustReconfigAll);

    public:

        MFSourceReader(const string &url, const ComPtr<IMFDXGIDeviceManager> &mfDXGIDevMan);

        std::chrono::microseconds GetDuration() const;

        void GetOutputMediaTypesFrom(DWORD idxStream, std::map<DWORD, DecodedMediaType> &decodedMTypes) const;

        void ReadSampleAsync();

        ComPtr<IMFSample> GetSample(DWORD &idxStream, DWORD &state);
    };

    enum class Encoder { H264_AVC, H265_HEVC };
    
    /// <summary>
    /// Wraps Media Foundation Sink Writer object.
    /// THIS IS NOT A THREAD SAFE IMPLEMENTATION!
    /// </summary>
    class MFSinkWriter : notcopiable
    {
    private:

        ComPtr<IMFSinkWriter> m_mfSinkWriter;

        enum MediaDataType : int16_t { Video, Audio };

        struct StreamInfo
        {
            uint16_t outIndex; // output stream index
            MediaDataType mediaDType; // media data type
        };

        /// <summary>
        /// A lookup table for fast conversion of index in source reader
        /// to sink writer. In order to favor performance of contiguos
        /// memory, there might be some unused positions.
        /// </summary>
        std::vector<StreamInfo> m_streamInfoLookupTab;

        /// <summary>
        /// Tracks gap occurences, kept as their timestamps and accessed
        /// in this array by their 0-based stream index.
        /// </summary>
        std::vector<LONGLONG> m_streamsGapsTracking;

        void AddStream(const ComPtr<IMFSinkWriterEx> &sinkWriterAltIntf,
                       DWORD idxDecStream,
                       const DecodedMediaType &decoded,
                       double targeSizeFactor,
                       Encoder encoder);

    public:

        MFSinkWriter(const string &url,
                     const ComPtr<IMFDXGIDeviceManager> &mfDXGIDevMan,
                     const std::map<DWORD, DecodedMediaType> &decodedMTypes,
                     double targeSizeFactor,
                     Encoder encoder);

        ~MFSinkWriter();

        void AddNewStreams(const std::map<DWORD, DecodedMediaType> &decodedMTypes,
                           double targeSizeFactor,
                           Encoder encoder);

        void EncodeSample(DWORD idxDecStream, const ComPtr<IMFSample> &sample);

        void PlaceGap(DWORD idxDecStream, LONGLONG timestamp);
    };

}// end of namespace application

#endif // end of header guard
