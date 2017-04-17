#include "stdafx.h"
#include "WicJpegTranscoder.h"
#include "MetadataCopier.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include <wincodecsdk.h>
#include <codecvt>
#include <iomanip>

namespace application
{
    using namespace _3fd::core;

    /// <summary>
    /// Initializes a new instance of the <see cref="WicJpegTranscoder"/> class.
    /// </summary>
    WicJpegTranscoder::WicJpegTranscoder()
    {
        CALL_STACK_TRACE;

        auto hr = CoCreateInstance(CLSID_WICImagingFactory,
                                   nullptr,
                                   CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(m_wicImagingFactory.GetAddressOf()));
        
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create imaging factory", "CoCreateInstance");
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="WicJpegTranscoder"/> class.
    /// </summary>
    WicJpegTranscoder::~WicJpegTranscoder()
    {
        MetadataCopier::Finalize();
    }

    /// <summary>
    /// Generates the name of the output file.
    /// </summary>
    /// <param name="inputFileName">Name of the input file.</param>
    /// <param name="isJXR">if set to <c>true</c>, generates a JPEG XR file name.</param>
    /// <returns>
    /// An UCS-2 encoded output file name.
    /// </returns>
    static std::wstring GenerateOutputFileName(const std::wstring &inputFileName, bool isJXR)
    {
        return std::wstring(inputFileName.begin(),
                            inputFileName.begin() + inputFileName.find_last_of(L'.'))
            + (isJXR ? L".jxr" : L"_R.jpg");
    }

    /// <summary>
    /// Evalates whether the source and destination formats are the same.
    /// </summary>
    /// <param name="decoder">The source image decoder.</param>
    /// <param name="encoder">The destination image encoder.</param>
    /// <returns>Whether formats are the same.</returns>
    static bool AreFormatsTheSame(IWICBitmapDecoder *decoder, IWICBitmapEncoder *encoder)
    {
        CALL_STACK_TRACE;

        HRESULT hr;

        GUID srcFormat;
        hr = decoder->GetContainerFormat(&srcFormat);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to retrieve container format", "IWICBitmapDecoder::GetContainerFormat");

        GUID destFormat;
        hr = encoder->GetContainerFormat(&destFormat);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to retrieve container format", "IWICBitmapEncoder::GetContainerFormat");

        return srcFormat == destFormat;
    }

    /// <summary>
    /// Configures the JPEG encoder options.
    /// </summary>
    /// <param name="propertyBag">The interface for its property bag.</param>
    /// <param name="imgQualityRatio">The image quality ratio, as a real number in the [0,1] interval.</param>
    static void ConfigureJpegEncoder(IPropertyBag2 *propertyBag, float imgQualityRatio)
    {
        CALL_STACK_TRACE;

        PROPBAG2 optImgQuality{};
        optImgQuality.pstrName = L"ImageQuality";
        VARIANT varImgQuality;
        VariantInit(&varImgQuality);
        varImgQuality.vt = VT_R4;
        varImgQuality.fltVal = imgQualityRatio;

        auto hr = propertyBag->Write(1, &optImgQuality, &varImgQuality);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to set image quality for transcoded frame", "IPropertyBag2::Write");
    }

    // Template function to copy metadata:

    template <typename IntfType1, typename IntfType2>
    void CopyMetadata(IntfType1 *source, IntfType2 *dest, bool sameFormat)
    {
        HRESULT hr;

        // When formats are the same, metadata can be copied directly:
        if (sameFormat)
        {
            ComPtr<IWICMetadataBlockReader> metadataBlockReader;
            hr = source->QueryInterface(IID_PPV_ARGS(metadataBlockReader.GetAddressOf()));
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to obtain metadata block reader", "QueryInterface");

            ComPtr<IWICMetadataBlockWriter> metadataBlockWriter;
            hr = dest->QueryInterface(IID_PPV_ARGS(metadataBlockWriter.GetAddressOf()));
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to obtain metadata block writer", "QueryInterface");

            hr = metadataBlockWriter->InitializeFromBlockReader(metadataBlockReader.Get());
            if (FAILED(hr))
            {
                WWAPI::RaiseHResultException(hr,
                    "Failed to copy image metadata associated to container",
                    "IWICMetadataBlockWriter::InitializeFromBlockReader");
            }

            return;
        }

        // Use metadata copier configured by XML:

        ComPtr<IWICMetadataQueryReader> metadataQueryReader;
        hr = source->GetMetadataQueryReader(metadataQueryReader.GetAddressOf());

        if (hr == WINCODEC_ERR_UNSUPPORTEDOPERATION)
            return;

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to obtain metadata query reader", "GetMetadataQueryReader");

        ComPtr<IWICMetadataQueryWriter> metadataQueryWriter;
        hr = dest->GetMetadataQueryWriter(metadataQueryWriter.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to obtain metadata query writer", "GetMetadataQueryWriter");

        MetadataCopier::GetInstance().Copy(metadataQueryReader.Get(), metadataQueryWriter.Get());
    }

    /// <summary>
    /// Copies the container (not frame) associated metadata from source to destination image.
    /// </summary>
    /// <param name="decoder">The image decoder.</param>
    /// <param name="encoder">The image encoder.</param>
    /// <param name="sameFormat">if set to <c>true</c>, source and destination image have the same format.</param>
    static void CopyContainerMetadata(IWICBitmapDecoder *decoder,
                                      IWICBitmapEncoder *encoder,
                                      bool sameFormat)
    {
        CALL_STACK_TRACE;
        CopyMetadata(decoder, encoder, sameFormat);
    }

    /// <summary>
    /// Copies the metadata from source to destination image frame.
    /// </summary>
    /// <param name="decodedFrame">The decoded frame from source.</param>
    /// <param name="transcodedFrame">The destination frame to receive transcoded data.</param>
    /// <param name="sameFormat">if set to <c>true</c>, source and destination image have the same format.</param>
    static void CopyFrameMetadata(IWICBitmapFrameDecode *decodedFrame,
                                  IWICBitmapFrameEncode *transcodedFrame,
                                  bool sameFormat)
    {
        CALL_STACK_TRACE;
        CopyMetadata(decodedFrame, transcodedFrame, sameFormat);
    }

    /// <summary>
    /// Transcodes the specified image file from any format supported by Windows to JPEG.
    /// </summary>
    /// <param name="fileName">Name of the file.</param>
    /// <param name="toJXR">if set to <c>true</c>, reencodes to JPEG XR.</param>
    /// <param name="imgQualityRatio">The image quality ratio, as a real number in the [0,1] interval.</param>
    void WicJpegTranscoder::Transcode(const string &fileName, bool toJXR, float imgQualityRatio)
    {
        CALL_STACK_TRACE;

        try
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> strConv;
            auto ucs2fInName = strConv.from_bytes(fileName);

            // Create decoder from input image file:

            HRESULT hr;
            ComPtr<IWICBitmapDecoder> decoder;
            hr = m_wicImagingFactory->CreateDecoderFromFilename(ucs2fInName.c_str(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                decoder.GetAddressOf());
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to create image decoder", "IWICImagingFactory::CreateDecoderFromFilename");

            // Create output file stream:

            ComPtr<IWICStream> fileOutStream;
            hr = m_wicImagingFactory->CreateStream(fileOutStream.GetAddressOf());
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to create file stream", "IWICImagingFactory::CreateStream");

            auto ucs2fOutName = GenerateOutputFileName(ucs2fInName, toJXR);
            hr = fileOutStream->InitializeFromFilename(ucs2fOutName.c_str(), GENERIC_WRITE);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to initialize output file stream", "IWICStream::InitializeFromFilename");

            // Create & initialized encoder:

            ComPtr<IWICBitmapEncoder> encoder;
            hr = m_wicImagingFactory->CreateEncoder(GUID_ContainerFormatWmp, nullptr, encoder.GetAddressOf());
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to create image encoder", "IWICImagingFactory::CreateEncoder");

            hr = encoder->Initialize(fileOutStream.Get(), WICBitmapEncoderCacheInMemory);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to initialize image encoder", "IWICBitmapEncoder::Initialize");

            // Copy the container thumbnail:

            ComPtr<IWICBitmapSource> ctnrThumbnail;
            hr = decoder->GetThumbnail(ctnrThumbnail.GetAddressOf());
            if (hr != WINCODEC_ERR_CODECNOTHUMBNAIL)
            {
                if (FAILED(hr))
                    WWAPI::RaiseHResultException(hr, "Failed to retrieve image container thumbnail", "IWICBitmapDecoder::GetThumbnail");

                hr = encoder->SetThumbnail(ctnrThumbnail.Get());
                if (FAILED(hr))
                    WWAPI::RaiseHResultException(hr, "Failed to copy image container thumbnail", "IWICBitmapEncoder::SetThumbnail");
            }

            bool toSameFormat = AreFormatsTheSame(decoder.Get(), encoder.Get());
            CopyContainerMetadata(decoder.Get(), encoder.Get(), toSameFormat);

            // Transcode frame data:

            UINT frameCount;
            hr = decoder->GetFrameCount(&frameCount);
            if (FAILED(hr))
                WWAPI::RaiseHResultException(hr, "Failed to retrieve image frame count", "IWICBitmapDecoder::GetFrameCount");

            // iterate over the image frames:
            for (UINT idx = 0; idx < frameCount; ++idx)
            {
                // decode the frame:
                ComPtr<IWICBitmapFrameDecode> decodedFrame;
                hr = decoder->GetFrame(idx, decodedFrame.GetAddressOf());
                if (FAILED(hr))
                    WWAPI::RaiseHResultException(hr, "Failed to decode image frame data", "IWICBitmapDecoder::GetFrame");

                // create frame to receive transcoded data:
                ComPtr<IPropertyBag2> encPropBag;
                ComPtr<IWICBitmapFrameEncode> transcodedFrame;
                hr = encoder->CreateNewFrame(transcodedFrame.GetAddressOf(), encPropBag.GetAddressOf());
                if (FAILED(hr))
                    WWAPI::RaiseHResultException(hr, "Failed to create image frame", "IWICBitmapEncoder::CreateNewFrame");

                // configure encoder for new frame:
                ConfigureJpegEncoder(encPropBag.Get(), imgQualityRatio);
                hr = transcodedFrame->Initialize(encPropBag.Get());
                if (FAILED(hr))
                    WWAPI::RaiseHResultException(hr, "Failed to set properties for transcoded image frame", "IWICBitmapFrameEncode::Initialize");

                // reencode
                hr = transcodedFrame->WriteSource(decodedFrame.Get(), nullptr);
                if (FAILED(hr))
                    WWAPI::RaiseHResultException(hr, "Failed to reencode bitmap", "IWICBitmapFrameEncode::WriteSource");

                // copy the frame thumbnail:
                ComPtr<IWICBitmapSource> thumbnail;
                hr = decodedFrame->GetThumbnail(thumbnail.GetAddressOf());
                if (hr != WINCODEC_ERR_CODECNOTHUMBNAIL)
                {
                    if (FAILED(hr))
                        WWAPI::RaiseHResultException(hr, "Failed to retrieve image frame thumbnail", "IWICBitmapFrameDecode::GetThumbnail");

                    hr = transcodedFrame->SetThumbnail(thumbnail.Get());
                    if (FAILED(hr))
                        WWAPI::RaiseHResultException(hr, "Failed to copy image frame thumbnail", "IWICBitmapFrameEncode::SetThumbnail");
                }

                CopyFrameMetadata(decodedFrame.Get(), transcodedFrame.Get(), toSameFormat);

                // commit to frame
                if (FAILED(hr = transcodedFrame->Commit()))
                    WWAPI::RaiseHResultException(hr, "Failed to commit transcoded image frame", "IWICBitmapFrameEncode::Commit");

            }// for loop end

             // commit to encoder
            if (FAILED(hr = encoder->Commit()))
                WWAPI::RaiseHResultException(hr, "Encoder failed to commit changes to transcoded image", "IWICBitmapEncoder::Commit");

            // commit to stream
            if (FAILED(hr = fileOutStream->Commit(STGC_DEFAULT)))
                WWAPI::RaiseHResultException(hr, "File stream failed to commit changes to storage", "IWICStream::Commit");
        }
        catch (IAppException &ex)
        {
            std::ostringstream oss;
            oss << "Failed to transcode image file " << fileName;
            throw AppException<std::runtime_error>(oss.str(), ex);
        }
    }

} // end of namespace application
