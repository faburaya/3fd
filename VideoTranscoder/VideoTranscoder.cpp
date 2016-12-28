// VideoTranscoder.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "3FD\runtime.h"
#include "3FD\exceptions.h"
#include "3FD\callstacktracer.h"
#include "3FD\logger.h"
#include "3FD\cmdline.h"
#include <iostream>

int main(int argc, char *argv[])
{
    using namespace _3fd;

    core::FrameworkInstance frameworkInstance;

    CALL_STACK_TRACE;

    // Initialize Windows Runtime API for COM usage of Microsoft Media Foundation
    auto hr = Windows::Foundation::Initialize(RO_INIT_MULTITHREADED);

    if (FAILED(hr))
    {
        std::cout << "Failed to initialize Windows Runtime API! " << core::WWAPI::GetDetailsFromHResult(hr);
        return -1;
    }

    try
    {
        using core::CommandLineArguments;

        CommandLineArguments cmdLineArgs(80, CommandLineArguments::ArgValSeparator::Colon, true, false);

        enum { ArgOptHelp, ArgValEncoder, ArgValTgtSizeFactor, ArgValsListIO };

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgOptHelp,
            CommandLineArguments::ArgType::OptionSwitch,
            CommandLineArguments::ArgPlacement::Anywhere,
            CommandLineArguments::ArgValType::None,
            'h', "help",
            "Displays these informations about usage of application command line arguments"
        });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValEncoder,
            CommandLineArguments::ArgType::OptionWithNonReqValue,
            CommandLineArguments::ArgPlacement::Anywhere,
            CommandLineArguments::ArgValType::EnumString,
            'e', "encoder",
            "What encoder to use, always with high profiles for better compression"
        }, { "h264", "hevc", "vc1" });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValTgtSizeFactor,
            CommandLineArguments::ArgType::OptionWithNonReqValue,
            CommandLineArguments::ArgPlacement::Anywhere,
            CommandLineArguments::ArgValType::RangeFloat,
            't', "target_size_factor",
            "The target size of the output transcoded video, as a fraction of the original size"
        }, { 0.5, 0.001, 1.0 });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValsListIO,
            CommandLineArguments::ArgType::ValuesList,
            CommandLineArguments::ArgPlacement::MustComeLast,
            CommandLineArguments::ArgValType::String,
            0, "input output",
            "input & output files"
        });

        cmdLineArgs.PrintArgsInfo();
    }
    catch (core::IAppException &ex)
    {
        core::Logger::Write(ex, core::Logger::PRIO_FATAL);
        return -2;
    }

    Windows::Foundation::Uninitialize();

    return 0;
}

