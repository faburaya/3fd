#ifndef MEDIAFOUNDATIONWRAPPERS_H // header guard
#define MEDIAFOUNDATIONWRAPPERS_H

#include "3FD\base.h"
#include <string>
#include <chrono>
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

        void GetOutputMediaTypesFrom(DWORD idxStream,
                                     std::map<DWORD, DecodedMediaType> &outMediaTypes,
                                     std::chrono::microseconds &duration) const;

        void ReadSampleAsync();

        enum ReadStateFlags
        {
            EndOfStream = MF_SOURCE_READERF_ENDOFSTREAM,
            NewStreamAvailable = MF_SOURCE_READERF_NEWSTREAM,
            NativeTypeChanged = MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED,
            GapFound = MF_SOURCE_READERF_STREAMTICK
        };

        ComPtr<IMFSample> GetSample(DWORD &state);
    };

    struct DecodedMediaType
    {
        UINT32 originalEncodedDataRate;
        ComPtr<IMFMediaType> mediaType;
    };

    enum class Encoder { H264_AVC, H265_HEVC };
    
    /// <summary>
    /// Wraps Media Foundation Sink Writer object.
    /// </summary>
    class MFSinkWriter : notcopiable
    {
    private:

        ComPtr<IMFSinkWriter> m_mfSinkWriter;
        std::map<DWORD, uint64_t> m_gapTrackByStreamIdx;

    public:

        MFSinkWriter(const string &url,
                     const ComPtr<IMFDXGIDeviceManager> &mfDXGIDevMan,
                     const std::map<DWORD, DecodedMediaType> &inMediaTypes,
                     double targeSizeFactor,
                     Encoder encoder);

        void WriteSample();

        void SendStreamTick();

        void Finalize();
    };

}// end of namespace application

#endif // end of header guard
