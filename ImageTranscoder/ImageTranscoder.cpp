// ImageTranscoder.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "3FD\runtime.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include "3FD\cmdline.h"
#include "MetadataCopier.h"
#include <iomanip>

namespace application
{
    //////////////////////////////
    // Command line arguments
    //////////////////////////////

    struct CmdLineParams
    {
        std::vector<const char *> dirPaths;
        double targetQuality;
        bool toJXR;
    };

    // Parse arguments from command line
    static bool ParseCommandLineArgs(int argc, const char *argv[], CmdLineParams &params)
    {
        CALL_STACK_TRACE;

        using _3fd::core::CommandLineArguments;

        CommandLineArguments cmdLineArgs(80, CommandLineArguments::ArgValSeparator::Colon, true, false);

        enum { ArgOptToJXR, ArgValTgtQuality, ArgValsListPaths };

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgOptToJXR,
            CommandLineArguments::ArgType::OptionSwitch,
            CommandLineArguments::ArgValType::None,
            'x', "jxr",
            "Whether the files should be transcoded to JPEG XR instead of regular JPEG"
        });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValTgtQuality,
            CommandLineArguments::ArgType::OptionWithReqValue,
            CommandLineArguments::ArgValType::RangeFloat,
            'q', "quality",
            "The target quality for the output transcoded image"
        }, { 0.95, 0.01, 1.0 });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValsListPaths,
            CommandLineArguments::ArgType::ValuesList,
            CommandLineArguments::ArgValType::String,
            0, "directories",
            "The paths for the directories whose contained files will be transcoded"
        }, { (uint16_t)1, (uint16_t)32 });

        const char *usageMessage("\nUsage:\n\n ImageTranscoder [/x] [/q:target_quality] path1 [path2 ...]\n\n");

        if (cmdLineArgs.Parse(argc, argv) == STATUS_FAIL)
        {
            std::cerr << usageMessage;
            cmdLineArgs.PrintArgsInfo();
            return STATUS_FAIL;
        }

        params.toJXR = cmdLineArgs.GetArgSwitchOptionValue(ArgOptToJXR);
        std::cout << '\n' << std::setw(22)
            << "output = " << (params.toJXR ? "JPEG XR" : "JPEG");

        bool isPresent;
        params.targetQuality = cmdLineArgs.GetArgValueFloat(ArgValTgtQuality, isPresent);

        // make it a percentual value, with steps of 1%
        params.targetQuality = floor(0.5 + params.targetQuality * 100) / 100;

        std::cout << '\n' << std::setw(22)
            << "target quality = " << params.targetQuality
            << (isPresent ? "\n\n" : " (default)\n\n");

        if (!cmdLineArgs.GetArgListOfValues(params.dirPaths))
        {
            std::cerr << "Must provide the directories with the files to transcode!\n" << usageMessage;
            cmdLineArgs.PrintArgsInfo();
            return STATUS_FAIL;
        }

        return STATUS_OKAY;
    }

}// end of namespace application

/////////////////////
// Entry Point
/////////////////////

int main(int argc, const char *argv[])
{
    using namespace _3fd::core;

    FrameworkInstance _framework(RO_INIT_MULTITHREADED);

    CALL_STACK_TRACE;

    try
    {
        application::CmdLineParams params;
        if (application::ParseCommandLineArgs(argc, argv, params) == STATUS_FAIL)
            return EXIT_FAILURE;

        auto &ref = application::MetadataCopier::GetInstance();
        application::MetadataCopier::Finalize();
    }
    catch (IAppException &ex)
    {
        Logger::Write(ex, Logger::PRIO_FATAL);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
