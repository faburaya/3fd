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

#ifdef _3FD_PLATFORM_WIN32API
#   define class_WicJpegTranscoder class WicJpegTranscoder : notcopiable
#   define _string const std::string &
#elif defined _3FD_PLATFORM_WINRT
#   define class_WicJpegTranscoder  public ref class WicJpegTranscoder sealed
#   define _string Platform::String ^
#endif

    /// <summary>
    /// Transcodes images from any format supported by Windows to JPEG.
    /// </summary>
    class_WicJpegTranscoder
    {
    private:

        ComPtr<IWICImagingFactory> m_wicImagingFactory;

    public:

        WicJpegTranscoder();
        virtual ~WicJpegTranscoder();

        void Transcode(_string filePath, bool toJXR, float imgQualityRatio);
    };

} // end of namespace application

#endif // end of header guard
