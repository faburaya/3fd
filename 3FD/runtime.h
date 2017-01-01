#ifndef RUNTIME_H
#define RUNTIME_H

#include "preprocessing.h"
#include "callstacktracer.h"
#include "logger.h"

#ifdef _3FD_PLATFORM_WIN32API
#   include <roapi.h>
#endif

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
    private:
#   ifdef _3FD_PLATFORM_WIN32API
        bool m_isComLibInitialized;
#   endif
	public:

		FrameworkInstance();
#   ifdef _3FD_PLATFORM_WIN32API
        FrameworkInstance(RO_INIT_TYPE comThreadModel);
#   endif
		~FrameworkInstance();
	};
}
}

#endif // header guard