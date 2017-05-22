#ifndef RUNTIME_H
#define RUNTIME_H

#include "preprocessing.h"
#include "callstacktracer.h"
#include "logger.h"
#include <string>

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
	class FrameworkInstance : notcopiable
	{
    private:

        std::string m_moduleName;

#ifdef _3FD_PLATFORM_WIN32API
        bool m_isComLibInitialized;
#endif
        
	public:

#ifdef _3FD_PLATFORM_WIN32API

        FrameworkInstance(RO_INIT_TYPE comThreadModel);
        FrameworkInstance();

#elif defined _3FD_PLATFORM_WINRT

        FrameworkInstance(const char *thisComName = "UNKNOWN");
#endif
		~FrameworkInstance();
	};

}// end of namespace core
}// end of namespace _3fd

#endif // header guard