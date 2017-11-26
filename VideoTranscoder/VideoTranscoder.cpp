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

    // Prints a pretty progress bar
    void PrintProgressBar(double progress)
    {
        // Amount of symbols = inside the progress bar
        const int qtBarSteps(50);

        static int done;

        // Only update the progress bar if there is a change:
        int update = (int)(qtBarSteps * progress);
        if (update == done)
            return;
        else
            done = update;

        // Print the progress bar:

        std::cout << "\rProgress: [";

        for (int idx = 0; idx < done; ++idx)
            std::cout << '#';

        int remaining = qtBarSteps - done;
        for (int idx = 0; idx < remaining; ++idx)
            std::cout << ' ';

        std::cout << "] " << static_cast<int> (100 * progress) << " % done" << std::flush;
    }

    //////////////////////////////
    // Command line arguments
    //////////////////////////////

    struct CmdLineParams
    {
        double tgtSizeFactor;
        Encoder encoder;
        const char *inputFName;
        const char *outputFName;
    };

    // Parse arguments from command line
    static bool ParseCommandLineArgs(int argc, const char *argv[], CmdLineParams &params)
    {
        CALL_STACK_TRACE;

        using _3fd::core::CommandLineArguments;

        CommandLineArguments cmdLineArgs(80, CommandLineArguments::ArgValSeparator::Colon, true, false);

        enum { ArgValEncoder, ArgValTgtSizeFactor, ArgValsListIO };

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValEncoder,
            CommandLineArguments::ArgType::OptionWithReqValue,
            CommandLineArguments::ArgValType::EnumString,
            'e', "encoder",
            "What encoder to use, always with the highest profile made available "
            "by Microsoft Media Foundation, for better compression"
        }, { "h264", "hevc" });

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
        const char *encoderLabel = cmdLineArgs.GetArgValueString(ArgValEncoder, isPresent);
        std::cout << '\n' << std::setw(22) << "encoder = " << encoderLabel
                  << (isPresent ? " " : " (default)");

        if (strcmp(encoderLabel, "h264") == 0)
            params.encoder = Encoder::H264_AVC;
        else if (strcmp(encoderLabel, "hevc") == 0)
            params.encoder = Encoder::H265_HEVC;
        else
            _ASSERTE(false);

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
                  << "output = " << params.outputFName << '\n' << std::endl;

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

    FrameworkInstance frameworkInstance(FrameworkInstance::ComMultiThreaded);

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

        // Load media source and select decoders
        application::MFSourceReader sourceReader(params.inputFName, mfDXGIDevMan);

        // Start reading early to avoid waiting
        sourceReader.ReadSampleAsync();

        // Gather info about reader output decoded streams:
        std::map<DWORD, application::DecodedMediaType> srcReadDecStreams;
        sourceReader.GetOutputMediaTypesFrom(0UL, srcReadDecStreams);
        
        if (srcReadDecStreams.empty())
        {
            std::cout << "\nInput media file had no video or audio streams to decode!\n" << std::endl;
            return EXIT_SUCCESS;
        }

        // Prepare media sink and select encoders
        application::MFSinkWriter sinkWriter(params.outputFName,
                                             mfDXGIDevMan,
                                             srcReadDecStreams,
                                             params.tgtSizeFactor,
                                             params.encoder);

        auto duration = sourceReader.GetDuration();
        std::cout << "\nInput media file is " << duration_cast<seconds>(duration).count() << " seconds long\n";

        std::array<char, 21> timestamp;
        auto now = system_clock::to_time_t(system_clock::now());
        strftime(timestamp.data(), timestamp.size(), "%Y-%b-%d %H:%M:%S", localtime(&now));
        std::cout << "Transcoding starting at " << timestamp.data() << '\n' << std::endl;

        application::PrintProgressBar(0.0);

        try
        {
            DWORD state;

            // Loop for transcoding (decoded source reader output goes to sink writer input):
            do
            {
                DWORD idxStream;
                auto sample = sourceReader.GetSample(idxStream, state);
                sourceReader.ReadSampleAsync();

                if (!sample)
                    continue;

                LONGLONG timestamp;
                sample->GetSampleTime(&timestamp);
                double progress = timestamp / (duration.count() * 10.0);
                application::PrintProgressBar(progress);

                if ((state & static_cast<DWORD> (application::ReadStateFlags::GapFound)) != 0)
                {
                    sinkWriter.PlaceGap(idxStream, timestamp);
                }
                else if ((state & static_cast<DWORD> (application::ReadStateFlags::NewStreamAvailable)) != 0)
                {
                    auto prevLastIdxStream = srcReadDecStreams.rbegin()->first;
                    sourceReader.GetOutputMediaTypesFrom(prevLastIdxStream + 1, srcReadDecStreams);
                    sinkWriter.AddNewStreams(srcReadDecStreams, params.tgtSizeFactor, params.encoder);
                }

                sinkWriter.EncodeSample(idxStream, sample);
            }
            while ((state & static_cast<DWORD> (application::ReadStateFlags::EndOfStream)) == 0);
        }
        catch (HResultException &ex)
        {
            // In case of "GPU device removed" error, log some additional info:
            if (ex.GetErrorCode() == DXGI_ERROR_DEVICE_REMOVED)
            {
                std::ostringstream oss;
                oss << "There a failure related to the GPU device: "
                    << WWAPI::GetDetailsFromHResult(d3dDevice->GetDeviceRemovedReason());

                Logger::Write(oss.str(), Logger::PRIO_FATAL);
            }

            throw;
        }

        application::PrintProgressBar(1.0);

        now = system_clock::to_time_t(system_clock::now());
        strftime(timestamp.data(), timestamp.size(), "%Y-%b-%d %H:%M:%S", localtime(&now));
        std::cout << "\n\nTranscoding finished at " << timestamp.data() << std::endl;
    }
    catch (IAppException &ex)
    {
        Logger::Write(ex, Logger::PRIO_FATAL);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
