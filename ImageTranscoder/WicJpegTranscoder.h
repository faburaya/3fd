#ifndef WICJPEGTRANSCODER_H // header guard
#define WICJPEGTRANSCODER_H

#include <string>
#include <wrl.h>
#include <wincodec.h>

namespace application
{
    using std::string;

    using namespace Microsoft::WRL;

    /// <summary>
    /// Transcodes images from any format supported by Windows to JPEG.
    /// </summary>
    class WicJpegTranscoder
    {
    private:

        ComPtr<IWICImagingFactory> m_wicImagingFactory;

    public:

        WicJpegTranscoder();

        void Transcode(const string &fileName, bool toJXR, float imgQualityRatio);
    };

} // end of namespace application

#endif // end of header guard
