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

int main(int argc, const char *argv[])
{
    using namespace _3fd;

    core::FrameworkInstance frameworkInstance(RO_INIT_MULTITHREADED);

    CALL_STACK_TRACE;

    try
    {
        application::CmdLineParams params;
        if (application::ParseCommandLineArgs(argc, argv, params) == STATUS_FAIL)
            return EXIT_FAILURE;

        application::MediaFoundationLib msmflib;
        application::MFSourceReader sourceReader(".\\sample.wmv", 0);
    }
    catch (core::IAppException &ex)
    {
        core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
