// ImageTranscoder.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "3FD\runtime.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include "3FD\cmdline.h"
#include <iostream>
#include <iomanip>

namespace application
{
    //////////////////////////////
    // Command line arguments
    //////////////////////////////

    struct CmdLineParams
    {
        std::vector<const char *> dirPaths;
        float targetQuality;
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
        std::cout << '\n' << std::setw(20)
            << "output = " << (params.toJXR ? "JPEG XR" : "JPEG");

        bool isPresent;
        auto paramValQuality = cmdLineArgs.GetArgValueFloat(ArgValTgtQuality, isPresent);

        // make it a percentual value, with steps of 1%
        params.targetQuality = static_cast<float> (floor(0.5 + paramValQuality * 100) / 100);

        std::cout << '\n' << std::setw(20)
            << "target quality = " << params.targetQuality
            << (isPresent ? "\n\n" : " (default)\n\n");

        if (!cmdLineArgs.GetArgListOfValues(params.dirPaths))
        {
            std::cerr << "Must provide the directories with files to transcode!\n" << usageMessage;
            cmdLineArgs.PrintArgsInfo();
            return STATUS_FAIL;
        }

        return STATUS_OKAY;
    }

}// end of namespace application

#include <boost\filesystem.hpp>
#include <algorithm>
#include <ctime>
#include "WicJpegTranscoder.h"

/////////////////////
// Entry Point
/////////////////////

int main(int argc, const char *argv[])
{
    using namespace _3fd::core;
    using namespace boost;

    FrameworkInstance _framework(RO_INIT_MULTITHREADED);

    CALL_STACK_TRACE;

    try
    {
        static const std::array<filesystem::path, 6> supImgFileExts = { L".tiff", L".jpeg", L".jpg", L".jxr", L".png", L".bmp" };

        application::CmdLineParams params;
        if (application::ParseCommandLineArgs(argc, argv, params) == STATUS_FAIL)
            return EXIT_FAILURE;

        unsigned int imgFilesTransCount(0);
        application::WicJpegTranscoder transcoder;

        auto startTime = clock();

        // Iterate over directories:
        for (int idx = 0; idx < params.dirPaths.size(); ++idx)
        {
            filesystem::path dirPath(params.dirPaths[idx]);

            if (!filesystem::exists(dirPath))
            {
                std::cerr << '\'' << dirPath.string() << "' does not exist!" << std::endl;
                continue;
            }
            else if (!filesystem::is_directory(dirPath))
            {
                std::cerr << '\'' << dirPath.string() << "' is not a directory!" << std::endl;
                continue;
            }

            std::cout << "Processing files in " << dirPath.string() << " ...\n";

            // Iterate over files in each directory:
            for (auto &dirEntry : filesystem::directory_iterator(dirPath))
            {
                // not a file?
                if (!filesystem::is_regular_file(dirEntry))
                    continue;
                // file without extension?
                else if (!dirEntry.path().has_extension())
                    continue;
                else
                {
                    auto fileExt = dirEntry.path().extension();

                    auto iter = std::find_if(
                        supImgFileExts.begin(),
                        supImgFileExts.end(),
                        [&fileExt](const filesystem::path &supExt) { return fileExt.compare(supExt) == 0; }
                    );

                    // not a supported extension?
                    if (supImgFileExts.end() == iter)
                        continue;
                }

                // transcode image file
                transcoder.Transcode(dirEntry.path().string(), params.toJXR, params.targetQuality);
                ++imgFilesTransCount;
            }

        }// outer for loop end

        auto endTime = clock();
        auto elapsedTime = static_cast<double>(endTime - startTime) / CLOCKS_PER_SEC;

        std::cout << "Successfully transcoded " << imgFilesTransCount << " image file(s) in "
                  << std::setprecision(3) << elapsedTime << " second(s)\n" << std::endl;
    }
    catch (filesystem::filesystem_error &ex)
    {
        Logger::Write("File system error!", ex.what(), Logger::PRIO_FATAL);
        return EXIT_FAILURE;
    }
    catch (IAppException &ex)
    {
        Logger::Write(ex, Logger::PRIO_FATAL);
        return EXIT_FAILURE;
    }
    catch (std::exception &ex)
    {
        std::ostringstream oss;
        oss << "Generic failure: " << ex.what();
        Logger::Write(oss.str(), Logger::PRIO_FATAL);
    }

    return EXIT_SUCCESS;
}
