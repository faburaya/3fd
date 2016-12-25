// VideoTranscoder.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "callstacktracer.h"
#include "exceptions.h"
#include <iostream>

int main(int argc, char *argv[])
{
    CALL_STACK_TRACE;

    using namespace _3fd;

    // Initialize Windows Runtime API for COM usage of Microsoft Media Foundation
    auto hr = Windows::Foundation::Initialize(RO_INIT_MULTITHREADED);

    if (FAILED(hr))
    {
        std::cout << "Failed to initialize Windows Runtime API! " << core::WWAPI::GetDetailsFromHResult(hr);
        return -1;
    }
    
    Windows::Foundation::Uninitialize();

    return 0;
}

