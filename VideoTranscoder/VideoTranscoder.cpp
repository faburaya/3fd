// VideoTranscoder.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "MediaFoundationWrappers.h"
#include "3FD\runtime.h"
#include "3FD\exceptions.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include "3FD\cmdline.h"

#include <iostream>
#include <iomanip>
#include <codecvt>
#include <mfapi.h>

namespace application
{
    using namespace _3fd::core;

    //////////////////////////////
    // Class MediaFoundationLib
    //////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="MediaFoundationLib"/> class.
    /// </summary>
    MediaFoundationLib::MediaFoundationLib()
    {
        CALL_STACK_TRACE;

        HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to initialize Microsoft Media Foundation platform", "MFStartup");
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="MediaFoundationLib"/> class.
    /// </summary>
    MediaFoundationLib::~MediaFoundationLib()
    {
        HRESULT hr = MFShutdown();

        if (FAILED(hr))
        {
            Logger::Write(hr,
                "Failed to shut down Microsoft Media Foundation platform",
                "MFShutdown",
                Logger::PRIO_CRITICAL
            );
        }
    }

    //////////////////////////////
    // Direct3D utilities
    //////////////////////////////

    /// <summary>
    /// Helps obtaining a Direct3D device for Microsoft DXGI.
    /// </summary>
    /// <param name="idxVideoAdapter">The index of the video adapter whose device will be in use by DXVA.</param>
    /// <returns>The Direct3D device.</returns>
    static ComPtr<ID3D11Device> GetDeviceDirect3D(UINT idxVideoAdapter)
    {
        CALL_STACK_TRACE;

        // Create DXGI factory:
        ComPtr<IDXGIFactory1> dxgiFactory;
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create DXGI factory", "CreateDXGIFactory1");

        // Get a video adapter:
        ComPtr<IDXGIAdapter1> dxgiAdapter;
        dxgiFactory->EnumAdapters1(idxVideoAdapter, dxgiAdapter.GetAddressOf());
        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to enumerate video adapters", "IDXGIAdapter1::EnumAdapters1");

        // Get video adapter description:
        DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
        dxgiAdapter->GetDesc1(&dxgiAdapterDesc);
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to retrieve DXGI video adapter description",
                "IDXGIAdapter1::GetDesc1"
            );
        }

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        std::cout << "Selected DXGI video adapter is \'"
            << transcoder.to_bytes(dxgiAdapterDesc.Description) << '\'' << std::endl;

        // Direct3D feature level codes and names:

        struct KeyValPair { int code; const char *name; };

        const KeyValPair d3dFLevelNames[] =
        {
            KeyValPair{ D3D_FEATURE_LEVEL_9_1, "Direct3D 9.1" },
            KeyValPair{ D3D_FEATURE_LEVEL_9_2, "Direct3D 9.2" },
            KeyValPair{ D3D_FEATURE_LEVEL_9_3, "Direct3D 9.3" },
            KeyValPair{ D3D_FEATURE_LEVEL_10_0, "Direct3D 10.0" },
            KeyValPair{ D3D_FEATURE_LEVEL_10_1, "Direct3D 10.1" },
            KeyValPair{ D3D_FEATURE_LEVEL_11_0, "Direct3D 11.0" },
            KeyValPair{ D3D_FEATURE_LEVEL_11_1, "Direct3D 11.1" },
        };

        // Feature levels for Direct3D support
        const D3D_FEATURE_LEVEL d3dFeatureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1,
        };

        constexpr auto nFeatLevels = static_cast<UINT> (
            (sizeof d3dFeatureLevels) / sizeof(D3D_FEATURE_LEVEL)
        );

        // Create Direct3D device:
        D3D_FEATURE_LEVEL featLevelCodeSuccess;
        ComPtr<ID3D11Device> d3dDx11Device;
        hr = D3D11CreateDevice(
            dxgiAdapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
            d3dFeatureLevels,
            nFeatLevels,
            D3D11_SDK_VERSION,
            d3dDx11Device.GetAddressOf(),
            &featLevelCodeSuccess,
            nullptr
        );

        // Might have failed for lack of Direct3D 11.1 runtime:
        if (hr == E_INVALIDARG)
        {
            // Try again without Direct3D 11.1:
            hr = D3D11CreateDevice(
                dxgiAdapter.Get(),
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                d3dFeatureLevels + 1,
                nFeatLevels - 1,
                D3D11_SDK_VERSION,
                d3dDx11Device.GetAddressOf(),
                &featLevelCodeSuccess,
                nullptr
            );
        }

        if (FAILED(hr))
            WWAPI::RaiseHResultException(hr, "Failed to create Direct3D device", "D3D11CreateDevice");

        // Get name of Direct3D feature level that succeeded upon device creation:
        auto d3dFLNameKVPairIter = std::find_if(
            d3dFLevelNames,
            d3dFLevelNames + nFeatLevels,
            [featLevelCodeSuccess](const KeyValPair &entry)
            {
                return entry.code == featLevelCodeSuccess;
            }
        );

        std::cout << "Hardware device supports " << d3dFLNameKVPairIter->name << std::endl;

        return d3dDx11Device;
    }
    
    //////////////////////////////
    // Command line arguments
    //////////////////////////////

    struct CmdLineParams
    {
        double tgtSizeFactor;
        string encoder;
        const char *inputFName;
        const char *outputFName;
    };

    // Parse arguments from command line
    static bool ParseCommandLineArgs(int argc, const char *argv[], CmdLineParams &params)
    {
        CALL_STACK_TRACE;

        using _3fd::core::CommandLineArguments;

        CommandLineArguments cmdLineArgs(80, CommandLineArguments::ArgValSeparator::Colon, true, false);

        enum { ArgOptHelp, ArgValEncoder, ArgValTgtSizeFactor, ArgValsListIO };

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgOptHelp,
            CommandLineArguments::ArgType::OptionSwitch,
            CommandLineArguments::ArgValType::None,
            'h', "help",
            "Displays these information about usage of application command line arguments"
        });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValEncoder,
            CommandLineArguments::ArgType::OptionWithReqValue,
            CommandLineArguments::ArgValType::EnumString,
            'e', "encoder",
            "What encoder to use, always with the highest profile made available "
            "by Microsoft Media Foundation, for better compression"
        }, { "h264", "hevc", "vc1" });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValTgtSizeFactor,
            CommandLineArguments::ArgType::OptionWithReqValue,
            CommandLineArguments::ArgValType::RangeFloat,
            't', "tsf",
            "The target size of the output transcoded video, as a fraction of the original size"
        }, { 0.5, 0.001, 1.0 });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValsListIO,
            CommandLineArguments::ArgType::ValuesList,
            CommandLineArguments::ArgValType::String,
            0, "input output",
            "input & output files"
        }, { (uint16_t)2, (uint16_t)2 });

        if (cmdLineArgs.Parse(argc, argv) == STATUS_FAIL)
        {
            std::cerr << "\nUsage:\n\n VideoTranscoder [/e:encoder] [/t:target_size_factor] input output\n\n";
            cmdLineArgs.PrintArgsInfo();
            return STATUS_FAIL;
        }

        bool isPresent;
        params.encoder = cmdLineArgs.GetArgValueString(ArgValEncoder, isPresent);
        std::cout << '\n' << std::setw(22) << "encoder = " << params.encoder
                  << (isPresent ? " " : " (default)");

        params.tgtSizeFactor = cmdLineArgs.GetArgValueFloat(ArgValTgtSizeFactor, isPresent);
        std::cout << '\n' << std::setw(22) << "target size factor = " << params.tgtSizeFactor
                  << (isPresent ? " " : " (default)");

        std::vector<const char *> filesNames;
        cmdLineArgs.GetArgListOfValues(filesNames);

        if (filesNames.size() != 2)
            return STATUS_FAIL;

        params.inputFName = filesNames[0];
        std::cout << '\n' << std::setw(22)
                  << "input = " << params.inputFName;

        params.outputFName = filesNames[1];
        std::cout << '\n' << std::setw(22)
                  << "input = " << params.outputFName << '\n' << std::endl;

        return STATUS_OKAY;
    }
    
}// end of namespace application

/////////////////
// Entry Point
/////////////////

using namespace Microsoft::WRL;

int main(int argc, const char *argv[])
{
    using namespace _3fd::core;

    FrameworkInstance frameworkInstance(RO_INIT_MULTITHREADED);

    CALL_STACK_TRACE;

    try
    {
        application::CmdLineParams params;
        if (application::ParseCommandLineArgs(argc, argv, params) == STATUS_FAIL)
            return EXIT_FAILURE;

        application::MediaFoundationLib msmflib;

        // Create an instance of the Microsoft DirectX Graphics Infrastructure (DXGI) Device Manager:
        ComPtr<IMFDXGIDeviceManager> mfDXGIDevMan;
        UINT dxgiResetToken;
        HRESULT hr = MFCreateDXGIDeviceManager(&dxgiResetToken, mfDXGIDevMan.GetAddressOf());
        if (FAILED(hr))
        {
            WWAPI::RaiseHResultException(hr,
                "Failed to create Microsoft DXGI Device Manager object",
                "MFCreateDXGIDeviceManager"
            );
        }

        // Get Direct3D device and associate with DXGI device manager:
        auto d3dDevice = application::GetDeviceDirect3D(0);
        mfDXGIDevMan->ResetDevice(d3dDevice.Get(), dxgiResetToken);

        application::MFSourceReader sourceReader(".\\sample.wmv", mfDXGIDevMan);
    }
    catch (IAppException &ex)
    {
        Logger::Write(ex, Logger::PRIO_CRITICAL);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
