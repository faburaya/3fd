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
#include <vector>
#include <string>
#include <ctime>
#include "WicJpegTranscoder.h"

void PrintProgressBar(uint32_t done, uint32_t total);

/////////////////////
// Entry Point
/////////////////////

int main(int argc, const char *argv[])
{
    using namespace _3fd::core;
    using namespace boost;

    auto startTime = clock();

    FrameworkInstance _framework(FrameworkInstance::ComMultiThreaded);

    CALL_STACK_TRACE;

    try
    {
        static const std::array<filesystem::path, 6> supImgFileExts = { L".tiff", L".jpeg", L".jpg", L".jxr", L".png", L".bmp" };

        application::CmdLineParams params;
        if (application::ParseCommandLineArgs(argc, argv, params) == STATUS_FAIL)
            return EXIT_FAILURE;

        std::vector<string> inputFiles;

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
                        [&fileExt](const filesystem::path &supExt)
                        {
                            string supExtStr = supExt.string();
                            string fileExtStr = fileExt.string();

                            return std::equal(
                                supExtStr.begin(), supExtStr.end(),
                                fileExtStr.begin(), fileExtStr.end(),
                                [](char chLow, char chX) { return chLow == tolower(chX); }
                            );
                        }
                    );

                    // not a supported extension?
                    if (supImgFileExts.end() == iter)
                        continue;
                }

                // keep this file path for later processing
                inputFiles.push_back(dirEntry.path().string());
            }

        }// outer for loop end

        if (inputFiles.empty())
        {
            std::cout << "There was no image file to transcode" << std::endl;
            return EXIT_SUCCESS;
        }

        application::WicJpegTranscoder transcoder;

        // transcode image file
        for (uint32_t idx = 0; idx < inputFiles.size(); ++idx)
        {
            auto &filePath = inputFiles[idx];
            transcoder.Transcode(filePath, params.toJXR, params.targetQuality);
            PrintProgressBar(idx + 1, inputFiles.size());
        }

        auto endTime = clock();
        auto elapsedTime = static_cast<double>(endTime - startTime) / CLOCKS_PER_SEC;

        std::cout << "\n\nSuccessfully transcoded " << inputFiles.size() << " image file(s) in "
                  << std::setprecision(3) << elapsedTime << " second(s)\n" << std::endl;

        return EXIT_SUCCESS;
    }
    catch (filesystem::filesystem_error &ex)
    {
        Logger::Write("File system error!", ex.what(), Logger::PRIO_FATAL);
    }
    catch (IAppException &ex)
    {
        std::cout << std::endl;
        Logger::Write(ex, Logger::PRIO_FATAL);
    }
    catch (std::exception &ex)
    {
        std::cout << std::endl;
        std::ostringstream oss;
        oss << "Generic failure: " << ex.what();
        Logger::Write(oss.str(), Logger::PRIO_FATAL);
    }

    return EXIT_FAILURE;
}

// Prints a pretty progress bar
void PrintProgressBar(uint32_t done, uint32_t total)
{
    const int numBarSteps(30);
    auto percentage = static_cast<double> (done) / total;
    auto nStepsDone = (int)(numBarSteps * percentage);

    std::cout << "\r[";

    for (int idx = 0; idx < nStepsDone; ++idx)
        std::cout << '#';

    int nStepsRemain = numBarSteps - nStepsDone;
    for (int idx = 0; idx < nStepsRemain; ++idx)
        std::cout << ' ';

    std::cout << "] " << std::setw(3) << std::right << static_cast<int> (100 * percentage + 0.5)
              << " % - " << done << " out of " << total
              << std::setprecision(10) << std::setw(10) << std::left << " files" << std::flush;
}