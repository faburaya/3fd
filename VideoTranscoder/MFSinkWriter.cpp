#include "stdafx.h"
#include "MediaFoundationWrappers.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include "3FD\utils.h"
#include <iostream>
#include <algorithm>
#include <codecvt>
#include <mfapi.h>

namespace application
{
    using namespace _3fd::core;

    /// <summary>
    /// Initializes a new instance of the <see cref="MFSinkWriter"/> class.
    /// </summary>
    /// <param name="url">The URL.</param>
    /// <param name="videoProps">The video props.</param>
    /// <param name="mfDXGIDevMan">The mf dxgi dev man.</param>
    MFSinkWriter::MFSinkWriter(const string &url,
                               const std::map<DWORD, VideoProperties> &videoProps,
                               const ComPtr<IMFDXGIDeviceManager> &mfDXGIDevMan)
    {
        CALL_STACK_TRACE;

        // Create attributes store, to set properties on the source reader:
        ComPtr<IMFAttributes> sinkReadAttrStore;
        HRESULT hr = MFCreateAttributes(sinkReadAttrStore.GetAddressOf(), 2);
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create attributes store", "MFCreateAttributes");

        // enable DXVA encoding
        sinkReadAttrStore->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, mfDXGIDevMan.Get());

        // enable codec hardware acceleration
        sinkReadAttrStore->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto ucs2url = transcoder.from_bytes(url);

        // Create sink writer:
        hr = MFCreateSinkWriterFromURL(ucs2url.c_str(),
                                       nullptr,
                                       sinkReadAttrStore.Get(),
                                       m_mfSinkWriter.GetAddressOf());

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create sink writer", "MFCreateSinkWriterFromURL");


    }

}// end of namespace application
