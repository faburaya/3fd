#pragma once

#include "3FD\runtime.h"
#include "WicJpegTranscoder.h"

namespace MyImagingComsWinRT
{
    public ref class JpegTranscoder sealed
    {
    private:

        _3fd::core::FrameworkInstance m_framework;
        application::WicJpegTranscoder m_transcoder;

    public:

        JpegTranscoder();

        void TranscodeSync(Platform::String ^filePath, bool toJXR, float imgQualityRatio);
    };

}// end of namespace media