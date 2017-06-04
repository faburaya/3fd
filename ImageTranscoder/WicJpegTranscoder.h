#ifndef WICJPEGTRANSCODER_H // header guard
#define WICJPEGTRANSCODER_H

#include "3FD\base.h"
#include "3FD\preprocessing.h"
#include <string>
#include <wrl.h>
#include <wincodec.h>

namespace application
{
    using namespace Microsoft::WRL;

    /// <summary>
    /// Transcodes images from any format supported by Windows to JPEG.
    /// </summary>
    class WicJpegTranscoder : notcopiable
    {
    private:

        ComPtr<IWICImagingFactory> m_wicImagingFactory;

    public:

        WicJpegTranscoder();
        virtual ~WicJpegTranscoder();

        void Transcode(const std::string &filePath, bool toJXR, float imgQualityRatio);

        void Transcode(IStream *inputStream, IStream *outputStream, bool toJXR, float imgQualityRatio);
    };


    std::wstring GenerateOutputFileName(const std::wstring &inputFileName, bool isJXR);


} // end of namespace application

#endif // end of header guard
