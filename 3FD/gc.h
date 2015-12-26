#ifndef GC_H
#define GC_H

#include "base.h"
#include "utils.h"
#include "gc_memorydigraph.h"
#include "utils_lockfreequeue.h"

#include <exception>
#include <thread>
#include <mutex>

/* Convention:

   Access point - a sptr object which does not belong to a region of memory managed 
   by the garbage collector. In other words, it is a sptr object that is not inside  
   any MemBlock region.
*/
namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// The interface that all GC messages must implement.
		/// </summary>
		class INTFOPT IMessage
		{
		public:

            virtual ~IMessage() {}

			virtual void Execute(MemoryDigraph &graph) = 0;
		};

		/// <summary>
		/// Implements the garbage collector engine.
		/// </summary>
		class GarbageCollector : notcopiable 
		{
		private:

			std::thread						m_thread;
			std::exception_ptr				m_error;
			MemoryDigraph					m_memoryDigraph;
            utils::LockFreeQueue<IMessage>	m_messagesQueue;
			utils::Event					m_terminationEvent;

			GarbageCollector();

			void GCThreadProc();

			// Singleton needs:

			static std::mutex		singleInstanceCreationMutex;
			static GarbageCollector	*uniqueObjectPtr;
			static GarbageCollector *CreateInstance();

		public:

			static GarbageCollector &GetInstance();

			static void Shutdown();

			~GarbageCollector();

			void RegisterNewObject(
				void *sptrObjAddr,
				void *pointedAddr,
				size_t blockSize,
				FreeMemProc freeMemCallback
			);

			void UpdateReference(void *leftSptrObjAddr, void *rightSptrObjAddr);

			void ReleaseReference(void *sptrObjAddr);

			void UnregisterAbortedObject(void *sptrObjAddr);

			void RegisterSptr(void *sptrObjAddr, void *pointedAddr);

			void RegisterSptrCopy(void *leftSptrObjAddr, void *rightSptrObjAddr);

			void UnregisterSptr(void *sptrObjAddr);
		};

	}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
