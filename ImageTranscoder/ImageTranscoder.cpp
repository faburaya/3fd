// ImageTranscoder.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "3FD\runtime.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"

/////////////////////
// Entry Point
/////////////////////

int main(int argc, char *argv[])
{
    using namespace _3fd::core;

    FrameworkInstance _framework(RO_INIT_MULTITHREADED);

    CALL_STACK_TRACE;

    try
    {

    }
    catch (IAppException &ex)
    {

    }

    return EXIT_SUCCESS;
}
