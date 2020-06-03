//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include <3fd/core/configuration.h>
#include <3fd/core/runtime.h>
#include <3fd/opencl/opencl.h>

#include <map>
#include <list>
#include <array>
#include <chrono>
#include <thread>
#include <atomic>
#include <future>
#include <random>
#include <iostream>

#define UNDEF_OCL_SRC_FILE "FILE PATH FOR OPENCL C SOURCE EXAMPLE IS NOT DEFINED IN XML CONFIGURATION"

namespace _3fd
{
namespace integration_tests
{
    using std::generate;
    using std::for_each;

    using namespace _3fd::core;

    void HandleException();

    static cl_device_type GetDeviceType()
    {
        return AppConfig::GetSettings().application.GetBool("testOclUseGpuDevice", false) ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU;
    }

#ifdef _WIN32
    static auto keyForOclWrongExampleFPath = "testOclWindowsWrongExampleFilePath";
    static auto keyForOclGoodExampleFPath = "testOclWindowsGoodExampleFilePath";
    static auto currentDirectory = ".\\";
#else
    static auto keyForOclWrongExampleFPath = "testOclLinuxWrongExampleFilePath";
    static auto keyForOclGoodExampleFPath = "testOclLinuxGoodExampleFilePath";
    static auto currentDirectory = "./";
#endif

    class OpenCL_TestCase : public ::testing::TestWithParam<bool>
	{
	private:

		// Ensures proper initialization/finalization of the framework
		_3fd::core::FrameworkInstance _framework;
	};

    /// <summary>
    /// Test the basics of the OpenCL module, including device discovery.
    /// </summary>
    TEST_F(OpenCL_TestCase, DeviceDiscovery_Test)
    {
        using namespace opencl;

        CALL_STACK_TRACE;

        try
        {
            // Platform::CreatePlatformInstances

            std::vector<Platform> platforms;
            Platform::CreatePlatformInstances(platforms);
            ASSERT_GT(platforms.size(), 0);

            // Platform::GetPlaformInfo

            std::array<char, 128> strValue;

            opencl::GenericParam param;
            param.Set(&strValue[0], strValue.size());
            platforms.back().GetPlatformInfo(CL_PLATFORM_NAME, param);
            std::cout << "\tPlatform name: " << &strValue[0] << std::endl;

            // Platform::GetContext
            auto context = platforms.back().CreateContextFromType(GetDeviceType());
            ASSERT_GT(context.GetNumDevices(), 0);
                
            // Context::GetDevices
            auto device = context.GetDevice(0, 0);
                
            // Device::GetDeviceInfo

            device->GetDeviceInfo(CL_DEVICE_NAME, param);
            std::cout << "\tDevice name: " << &strValue[0] << std::endl;

            device->GetDeviceInfo(CL_DEVICE_VENDOR, param);
            std::cout << "\tDevice vendor: " << &strValue[0] << std::endl;

			cl_uint maxComputeUnits;
			param.Set(maxComputeUnits);
			device->GetDeviceInfo(CL_DEVICE_MAX_COMPUTE_UNITS, param);
			std::cout << "\tDevice compute units: " << maxComputeUnits << std::endl;

			cl_uint maxWorkItemDimensions;
			param.Set(maxWorkItemDimensions);
			device->GetDeviceInfo(CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, param);
			std::cout << "\tDevice max work item dimensions: " << maxWorkItemDimensions << std::endl;

			cl_ulong maxWorkGroupSize;
			param.Set(maxWorkGroupSize);
			device->GetDeviceInfo(CL_DEVICE_MAX_WORK_GROUP_SIZE, param);
			std::cout << "\tDevice max workgroup size: " << maxWorkGroupSize << std::endl;
        }
        catch(...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Test the OpenCL module for program compilation.
    /// </summary>
    TEST_F(OpenCL_TestCase, ProgramCompilation_Test)
    {
        using namespace opencl;

        CALL_STACK_TRACE;

        try
        {
            std::vector<Platform> platforms;
            Platform::CreatePlatformInstances(platforms);
            ASSERT_GT(platforms.size(), 0);

            auto context = platforms.back().CreateContextFromType(GetDeviceType());

            try
            {
                // Build a wrecked kernel source code
                context.BuildProgramFromSource(
                    AppConfig::GetSettings().application.GetString(keyForOclWrongExampleFPath, UNDEF_OCL_SRC_FILE), ""
                );
            }
            catch(IAppException &ex)
            {
                Logger::Write(ex, core::Logger::PRIO_ERROR);
            }

            // Build a correct kernel source code
            auto program = context.BuildProgramFromSource(
                AppConfig::GetSettings().application.GetString(keyForOclGoodExampleFPath, UNDEF_OCL_SRC_FILE), ""
            );

            // Save binaries to disk:
            auto manifestFilePath = program->SaveAs("example", currentDirectory);

            program.reset(); // discard the built program

            // Reload program from its binary files:
            program = context.BuildProgramWithBinaries(manifestFilePath, "");

            // Create a kernel after the program compilation
            auto kernel = program->CreateKernel("transform");
        }
        catch(...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests the OpenCL module for synchronous read/write operations on buffers.
    /// </summary>
    TEST_P(OpenCL_TestCase, BufferRW_SyncOps_Test)
    {
        using namespace opencl;

        CALL_STACK_TRACE;

        try
        {
            std::vector<Platform> platforms;
            Platform::CreatePlatformInstances(platforms);
            ASSERT_GT(platforms.size(), 0);

            auto context = platforms.back().CreateContextFromType(GetDeviceType());
            ASSERT_GT(context.GetNumDevices(), 0);

            auto device = context.GetDevice(0, GetParam() ? CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE : 0);

            // Host buffer:
            std::array<float, 1024> hostBuffer1, hostBuffer2;
            hostBuffer1.fill(66.6F);
            const size_t commonBufSize = hostBuffer1.size() * sizeof hostBuffer1[0];

            // Input/Output device buffer:
            auto buffer = context.CreateBuffer(commonBufSize, CL_MEM_COPY_HOST_PTR, hostBuffer1.data());

            // Did the initialization went okay?
            device->EnqueueReadBuffer(buffer, 0, commonBufSize, hostBuffer2.data());

            EXPECT_EQ(66.6f, hostBuffer2[0]);
            EXPECT_EQ(66.6f, hostBuffer2[255]);
            EXPECT_EQ(66.6f, hostBuffer2[511]);
            EXPECT_EQ(66.6f, hostBuffer2[767]);
            EXPECT_EQ(66.6f, hostBuffer2[1023]);

            // Now write something else:
            hostBuffer2.fill(99.9F);
            device->EnqueueWriteBuffer(buffer, 0, commonBufSize, hostBuffer2.data());

            // Did the writing went as expected?
            device->EnqueueReadBuffer(buffer, 0, commonBufSize, hostBuffer1.data());

            EXPECT_EQ(99.9f, hostBuffer1[0]);
            EXPECT_EQ(99.9f, hostBuffer1[255]);
            EXPECT_EQ(99.9f, hostBuffer1[511]);
            EXPECT_EQ(99.9f, hostBuffer1[767]);
            EXPECT_EQ(99.9f, hostBuffer1[1023]);
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests the OpenCL module for asynchronous read/write operations on buffers.
    /// </summary>
    TEST_P(OpenCL_TestCase, BufferRW_AsyncOps_Test)
    {
        using namespace opencl;

        CALL_STACK_TRACE;

        try
        {
            std::vector<Platform> platforms;
            Platform::CreatePlatformInstances(platforms);
            ASSERT_GT(platforms.size(), 0);

            auto context = platforms.back().CreateContextFromType(GetDeviceType());
            ASSERT_GT(context.GetNumDevices(), 0);

            auto device = context.GetDevice(0, GetParam() ? CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE : 0);

            // Host data:
            std::array<int, 1024> hostBuffer1, hostBuffer2;
            const size_t commonBufSize = hostBuffer1.size() * sizeof hostBuffer1[0];

            // Output device buffer:
            auto outputBuffer = context.CreateBuffer(commonBufSize, CL_MEM_WRITE_ONLY, nullptr);

            // Input buffer
            auto inputBuffer = context.CreateBuffer(commonBufSize, CL_MEM_READ_ONLY, nullptr);

            // Fill the input buffer with a pattern of integers:
            std::array<int, 4> pattern = { 1, 2, 3, 4 };
            GenericParam param;
            param.Set(pattern.data(), pattern.size() * sizeof pattern[0]);
            device->EnqueueFillBufferAsync(inputBuffer, 0, inputBuffer.Size() / param.size, param).Detach();

            // Copy the contents from one buffer to another
            device->EnqueueCopyBufferAsync(inputBuffer, outputBuffer, 0, 0, commonBufSize).Detach();

            // Read the destination to check its content against the original content:
            auto asyncRead = device->EnqueueReadBufferAsync(inputBuffer, 0, commonBufSize, hostBuffer1.data());
                
            asyncRead.Await();
            EXPECT_EQ(pattern[0], hostBuffer1[0]);
            EXPECT_EQ(pattern[1], hostBuffer1[1]);
            EXPECT_EQ(pattern[2], hostBuffer1[2]);
            EXPECT_EQ(pattern[3], hostBuffer1[3]);
            EXPECT_EQ(pattern[0], hostBuffer1[512]);
            EXPECT_EQ(pattern[1], hostBuffer1[513]);
            EXPECT_EQ(pattern[2], hostBuffer1[514]);
            EXPECT_EQ(pattern[3], hostBuffer1[515]);

            // Now write something else to the input buffer:
            hostBuffer1.fill(696);
            device->EnqueueWriteBufferAsync(inputBuffer, 0, commonBufSize, hostBuffer1.data()).Detach();

            // Copy the contents from one buffer to another
            device->EnqueueCopyBufferAsync(inputBuffer, outputBuffer, 0, 0, commonBufSize).Detach();

            // Read the destination to check its content against the original content:
            asyncRead = device->EnqueueReadBufferAsync(inputBuffer, 0, commonBufSize, hostBuffer2.data());

            asyncRead.Await();
            EXPECT_EQ(hostBuffer1[0], hostBuffer2[0]);
            EXPECT_EQ(hostBuffer1[255], hostBuffer2[255]);
            EXPECT_EQ(hostBuffer1[511], hostBuffer2[511]);
            EXPECT_EQ(hostBuffer1[767], hostBuffer2[767]);
            EXPECT_EQ(hostBuffer1[1023], hostBuffer2[1023]);
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests the OpenCL module for asynchronous read/write operations on buffers.
    /// </summary>
    TEST_P(OpenCL_TestCase, BufferMap_SyncOps_Test)
    {
        using namespace opencl;

        CALL_STACK_TRACE;

        try
        {
            std::vector<Platform> platforms;
            Platform::CreatePlatformInstances(platforms);
            ASSERT_GT(platforms.size(), 0);

            auto context = platforms.back().CreateContextFromType(GetDeviceType());
            ASSERT_GT(context.GetNumDevices(), 0);

            auto device = context.GetDevice(0, GetParam() ? CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE : 0);

            // Device buffer:
            const size_t bufSize = sizeof(float) * 1024;
            auto buffer = context.CreateBuffer(bufSize, CL_MEM_ALLOC_HOST_PTR, nullptr);

            // Map the buffer so as to write into it:
            device->EnqueueMapBuffer(buffer, MemResourceUse::Output, 0, bufSize,
                [](void *ptr, size_t nBytes)
                {
                    auto array = static_cast<float *> (ptr);
                    for (int idx = 0; idx < nBytes / sizeof(float); ++idx)
                        array[idx] = 69.6F;
                });

            // Now, map it again to read its content and change it:
            device->EnqueueMapBuffer(buffer, MemResourceUse::InputAndOutput, 0, bufSize,
                [](void *ptr, size_t nBytes)
                {
                    auto array = static_cast<float *> (ptr);
                    for (int idx = 0; idx < nBytes / sizeof(float); ++idx)
                        array[idx] *= 10;
                });

            // Finally map the buffer just to check its content
            bool good(false);
            device->EnqueueMapBuffer(buffer, MemResourceUse::Input, 0, bufSize,
                [&good](void *ptr, size_t nBytes)
                {
                    auto array = static_cast<float *> (ptr);
                    const auto end = nBytes / sizeof(float);
                    int idx = 0;
                    while (idx < end && array[idx] == 696.0F)
                        ++idx;

                    good = (idx == end);
                });

            EXPECT_TRUE(good);
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests the OpenCL module for asynchronous read/write operations on buffers.
    /// </summary>
    TEST_P(OpenCL_TestCase, BufferMap_AsyncOps_Test)
    {
        using namespace opencl;

        CALL_STACK_TRACE;

        try
        {
            std::vector<Platform> platforms;
            Platform::CreatePlatformInstances(platforms);
            ASSERT_GT(platforms.size(), 0);

            auto context = platforms.back().CreateContextFromType(GetDeviceType());
            ASSERT_GT(context.GetNumDevices(), 0);

            auto device = context.GetDevice(0, GetParam() ? CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE : 0);

            // Device buffer:
            const size_t bufSize = sizeof(float) * 1024;
            auto buffer = context.CreateBuffer(bufSize, CL_MEM_ALLOC_HOST_PTR, nullptr);

            // Map the buffer so as to write into it:
            device->EnqueueMapBufferAsync(buffer, MemResourceUse::Output, 0, bufSize,
                [](void *ptr, size_t nBytes)
                {
                    auto array = static_cast<float *> (ptr);
                    for (int idx = 0; idx < nBytes / sizeof(float); ++idx)
                        array[idx] = 69.6F;
                })
                .Detach();

            // Now, map it again to read its content and change it:
            device->EnqueueMapBufferAsync(buffer, MemResourceUse::InputAndOutput, 0, bufSize,
                [](void *ptr, size_t nBytes)
                {
                    auto array = static_cast<float *> (ptr);
                    for (int idx = 0; idx < nBytes / sizeof(float); ++idx)
                        array[idx] *= 10;
                })
                .Detach();

            // Finally map the buffer just to check its content
            std::atomic<bool> good(false);
            device->EnqueueMapBufferAsync(buffer, MemResourceUse::Input, 0, bufSize,
                [&good](void *ptr, size_t nBytes)
                {
                    auto array = static_cast<float *> (ptr);
                    const auto end = nBytes / sizeof(float);
                    int idx = 0;
                    while (idx < end && array[idx] == 696.0F)
                        ++idx;

                    if (idx == end)
                        good.store(true, std::memory_order_release);
                })
                .Await();

            EXPECT_TRUE(good.load(std::memory_order_acquire));
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests the OpenCL module for kernel execution and data transfer from and to the buffers.
    /// </summary>
    TEST_P(OpenCL_TestCase, KernelExecution_Test)
    {
        using namespace opencl;

        CALL_STACK_TRACE;

        try
        {
            std::vector<Platform> platforms;
            Platform::CreatePlatformInstances(platforms);
            ASSERT_GT(platforms.size(), 0);

            auto context = platforms.back().CreateContextFromType(GetDeviceType());
            ASSERT_GT(context.GetNumDevices(), 0);

            auto device = context.GetDevice(0, GetParam() ? CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE : 0);

            /* Try to build the program using the files from a previous compilation, but if that 
            does not work, build it again from source code: */
            std::unique_ptr<Program> program = context.BuildProgramWithBinaries("ocl_manifest_example.xml", "");
            if (program.get() == nullptr)
            {
                program = context.BuildProgramFromSource(
                    AppConfig::GetSettings().application.GetString(keyForOclGoodExampleFPath, UNDEF_OCL_SRC_FILE), ""
                );

                program->SaveAs("example", currentDirectory);
            }
                
            auto kernel = program->CreateKernel("transform");

            // Host buffers:
            std::array<float, 1024> hostData1, hostData2;
            hostData1.fill(66.6F);
            hostData2.fill(99.9F);

            const size_t commonBufSize = hostData1.size() * sizeof(hostData1[0]);

            // Device input buffer. Data is copied into it as of creation:
            auto inputBuffer1 = context.CreateBuffer(commonBufSize,
                                                     CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_WRITE_ONLY, 
                                                     hostData1.data());

            // Device input buffer. Data is copied into it as of creation:
            auto inputBuffer2 = context.CreateBuffer(commonBufSize,
                                                     CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_WRITE_ONLY,
                                                     hostData2.data());
            // First device output buffer:
            auto outputBuffer1 = context.CreateBuffer(commonBufSize,
                                                      CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY, 
                                                      nullptr);
            // Seconde device output buffer:
            auto outputBuffer2 = context.CreateBuffer(commonBufSize,
                                                      CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY,
                                                      nullptr);

            // Enqueues the execution of the kernel using the first input and output buffers:
            std::array<size_t, 1> globalWorkOffset = { 0 };
            std::array<size_t, 1> globalWorkSize = { hostData1.size() };
            std::array<size_t, 1> localWorkSize = { 256 };

            kernel.SetKernelArg(0, inputBuffer1, MemResourceUse::Input);
            kernel.SetKernelArg(1, outputBuffer1, MemResourceUse::Output);

            device->EnqueueNDRangeKernelAsync<1>(
                kernel, 
                globalWorkOffset, 
                globalWorkSize, 
                localWorkSize
            )
            .Detach();

            // Enqueues the execution of the kernel using the second input and output buffers:
            kernel.SetKernelArg(0, inputBuffer2, MemResourceUse::Input);
            kernel.SetKernelArg(1, outputBuffer2, MemResourceUse::Output);

            device->EnqueueNDRangeKernelAsync<1>(
                kernel,
                globalWorkOffset,
                globalWorkSize,
                localWorkSize
            )
            .Detach();

            // Read the results in the first output buffer:
            device->EnqueueReadBufferAsync(
                outputBuffer1, 
                0, commonBufSize, 
                hostData1.data()
            )
            .Detach();

            // Read the results in the second output buffer:
            device->EnqueueReadBufferAsync(
                outputBuffer2,
                0, commonBufSize,
                hostData2.data()
            )
            .Detach();

            // Wait completion of all queued commands
            device->Finish();

            // Check results:
            EXPECT_EQ(666.0f, hostData1[0]);
            EXPECT_EQ(666.0f, hostData1[255]);
            EXPECT_EQ(666.0f, hostData1[511]);
            EXPECT_EQ(666.0f, hostData1[767]);
            EXPECT_EQ(666.0f, hostData1[1023]);

            EXPECT_EQ(999.0f, hostData2[0]);
            EXPECT_EQ(999.0f, hostData2[255]);
            EXPECT_EQ(999.0f, hostData2[511]);
            EXPECT_EQ(999.0f, hostData2[767]);
            EXPECT_EQ(999.0f, hostData2[1023]);
        }
        catch(...)
        {
            HandleException();
        }
    }

    INSTANTIATE_TEST_CASE_P(Switch_InOrder_OutOfOrder, 
							OpenCL_TestCase,
                            ::testing::Values(false, true));

}// end of namespace integration_tests
}// end of namespace _3fd
