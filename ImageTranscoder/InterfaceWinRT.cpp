#include "stdafx.h"
#include "InterfaceWinRT.h"
#include "3FD\utils_winrt.h"
#include "3FD\exceptions.h"
#include "3FD\callstacktracer.h"
#include "3FD\logger.h"
#include <codecvt>
#include <robuffer.h>
#include <shcore.h>

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


    using namespace Microsoft::WRL;
    using namespace Windows::Storage;
    using namespace Windows::Storage::Streams;


    /// <summary>
    /// Transcodes the specified image file from any format supported by Windows to JPEG.
    /// </summary>
    /// <param name="inputFile">The image file to transcode.</param>
    /// <param name="toJXR">if set to <c>true</c>, reencodes to JPEG XR.</param>
    /// <param name="imgQualityRatio">The image quality ratio, as a real number in the [0,1] interval.</param>
    void JpegTranscoder::TranscodeSync(StorageFile ^inputFile, bool toJXR, float imgQualityRatio)
    {
        CALL_STACK_TRACE;

        using namespace core;

        try
        {
            // Create input stream:

            auto winrtInputStream = utils::WinRTExt::WaitForAsync(inputFile->OpenAsync(FileAccessMode::Read));

            HRESULT hr;
            ComPtr<IStream> oleInputStream;
            hr = CreateStreamOverRandomAccessStream(winrtInputStream, IID_PPV_ARGS(oleInputStream.GetAddressOf()));
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to create input IStream interface from WinRT random access stream",
                    "CreateStreamOverRandomAccessStream");
            }

            // Create output stream:

            std::wstring outFilePath = application::GenerateOutputFileName(inputFile->Path->Data(), toJXR);
            
            auto outFNameSubCStr = wcsrchr(outFilePath.c_str(), L'\\') + 1;

            std::wstring dirPath(outFilePath.c_str(), outFNameSubCStr);

            auto folder = utils::WinRTExt::WaitForAsync(
                StorageFolder::GetFolderFromPathAsync(Platform::StringReference(dirPath.c_str()))
            );

            auto outputFile = utils::WinRTExt::WaitForAsync(
                folder->CreateFileAsync(Platform::StringReference(outFNameSubCStr))
            );

            auto winrtOutputStream = utils::WinRTExt::WaitForAsync(outputFile->OpenAsync(FileAccessMode::ReadWrite));
            
            ComPtr<IStream> oleOutputStream;
            hr = CreateStreamOverRandomAccessStream(winrtOutputStream, IID_PPV_ARGS(oleOutputStream.GetAddressOf()));
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to create output IStream interface from WinRT random access stream",
                    "CreateStreamOverRandomAccessStream");
            }

            // and finally transcode
            m_transcoder.Transcode(oleInputStream.Get(), oleOutputStream.Get(), toJXR, imgQualityRatio);
        }
        catch (Platform::Exception ^ex)
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
            std::ostringstream oss;
            oss << "Failed to transcode image file " << strConv.to_bytes(inputFile->Path->Data());
            auto appEx = AppException<std::runtime_error>(oss.str(), WWAPI::GetDetailsFromWinRTEx(ex));
            Logger::Write(appEx, core::Logger::PRIO_CRITICAL);
            throw ref new Platform::Exception(E_FAIL);
        }
        catch (core::IAppException &ex)
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
            std::ostringstream oss;
            oss << "Failed to transcode image file " << strConv.to_bytes(inputFile->Path->Data());
            auto wrapEx = AppException<std::runtime_error>(oss.str(), ex);
            Logger::Write(wrapEx, core::Logger::PRIO_CRITICAL);
            throw ref new Platform::Exception(E_FAIL);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when transcoding image file: " << ex.what();
            Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL, true);
            throw ref new Platform::Exception(E_FAIL);
        }
    }

}// end of namespace MyImagingComsWinRT
