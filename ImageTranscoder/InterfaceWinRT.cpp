#include "stdafx.h"
#include "InterfaceWinRT.h"
#include "3FD\exceptions.h"
#include "3FD\callstacktracer.h"
#include "3FD\logger.h"
#include <sstream>
#include <codecvt>

namespace MyImagingComsWinRT
{
    using namespace _3fd;

    /// <summary>
    /// Initializes a new instance of the <see cref="JpegTranscoder"/> class.
    /// </summary>
    JpegTranscoder::JpegTranscoder()
    try :
        m_framework("MyImagingComsWinRT"),
        m_transcoder()
    {
    }
    catch (core::IAppException &ex)
    {
        CALL_STACK_TRACE;
        core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
        throw ref new Platform::Exception(E_FAIL);
    }

    /// <summary>
    /// Transcodes the specified image file from any format supported by Windows to JPEG.
    /// </summary>
    /// <param name="filePath">Full name of the file.</param>
    /// <param name="toJXR">if set to <c>true</c>, reencodes to JPEG XR.</param>
    /// <param name="imgQualityRatio">The image quality ratio, as a real number in the [0,1] interval.</param>
    void JpegTranscoder::TranscodeSync(Platform::String ^filePath, bool toJXR, float imgQualityRatio)
    {
        CALL_STACK_TRACE;

        try
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
            const auto utf8FilePath = strConv.to_bytes(filePath->Data());
            m_transcoder.Transcode(utf8FilePath, toJXR, imgQualityRatio);
        }
        catch (core::IAppException &ex)
        {
            core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
            throw ref new Platform::Exception(E_FAIL);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when transcoding image file: " << ex.what();
            core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL, true);
            throw ref new Platform::Exception(E_FAIL);
        }
    }

}// end of namespace MyImagingComsWinRT
