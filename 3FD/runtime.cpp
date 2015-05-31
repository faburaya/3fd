#include "stdafx.h"
#include "runtime.h"
#include "callstacktracer.h"
#include "logger.h"
#include "gc.h"

namespace _3fd
{
	namespace core
	{
		////////////////////////////////////
		//  FrameworkInstance Class
		////////////////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="FrameworkInstance" /> class.
		/// </summary>
		FrameworkInstance::FrameworkInstance()
		{
			Logger::Write("3FD has been initialized", core::Logger::PRIO_DEBUG);
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="FrameworkInstance"/> class.
		/// </summary>
		FrameworkInstance::~FrameworkInstance()
		{
			memory::GarbageCollector::Shutdown();
			CallStackTracer::Shutdown();

			Logger::Write("3FD was shutdown", core::Logger::PRIO_DEBUG);
			Logger::Shutdown();
		}
	}
}