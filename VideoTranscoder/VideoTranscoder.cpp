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

        const char *usageMessage("\nUsage:\n\n VideoTranscoder [/e:encoder] [/t:target_size_factor] input output\n\n");

        if (cmdLineArgs.Parse(argc, argv) == STATUS_FAIL)
        {
            std::cerr << usageMessage;
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
        {
            std::cerr << "\nMust provide input & output files!\n" << usageMessage;
            cmdLineArgs.PrintArgsInfo();
            return STATUS_FAIL;
        }

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
    using namespace std::chrono;
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
                "MFCreateDXGIDeviceManager");
        }

        // Get Direct3D device and associate with DXGI device manager:
        auto d3dDevice = application::GetDeviceDirect3D(0);
        mfDXGIDevMan->ResetDevice(d3dDevice.Get(), dxgiResetToken);

        application::MFSourceReader sourceReader(params.outputFName, mfDXGIDevMan);
    }
    catch (IAppException &ex)
    {
        Logger::Write(ex, Logger::PRIO_CRITICAL);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
