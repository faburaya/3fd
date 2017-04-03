// ImageTranscoder.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "3FD\runtime.h"
#include "3FD\callstacktracer.h"
#include "3FD\exceptions.h"
#include "3FD\logger.h"
#include "MetadataCopier.h"

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
        application::MetadataCopier metaDataCopier("MetadataCopyMap.xml");
    }
    catch (IAppException &ex)
    {
        Logger::Write(ex, Logger::PRIO_FATAL);
    }

    return EXIT_SUCCESS;
}
