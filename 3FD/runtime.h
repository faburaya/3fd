#ifndef RUNTIME_H
#define RUNTIME_H

#include "preprocessing.h"
#include "callstacktracer.h"
#include "logger.h"

namespace _3fd
{
	namespace core
	{
		/// <summary>
		/// An object to be allocated on the stack which is responsible for 
		/// starting or stopping the framework instance for the current thread.
		/// </summary>
		class FrameworkInstance
		{
		public:

			FrameworkInstance();
			~FrameworkInstance();
		};
	}
}

#endif // header guard