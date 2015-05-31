#ifndef GC_COMMON_H
#define GC_COMMON_H

#include <cstdlib>

namespace _3fd
{
	namespace memory
	{
		typedef void (*FreeMemProc)(void *addr, bool destroy);

		void *AllocMemoryAndRegisterWithGC(
			size_t size,
			void *sptrObjAddr,
			FreeMemProc freeMemCallback);

	}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
