#ifndef GC_MESSAGES_H // header guard
#define GC_MESSAGES_H

#include "gc.h"

namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// Message used to inform that a <see cref="sptr"/> object is now referencing a different/new object.
		/// </summary>
		class ReferenceUpdateMsg : public IMessage
		{
		private:

			void *m_sptrObjAddr,
				 *m_pointedAddr;

		public:

			ReferenceUpdateMsg(void *sptrObjAddr, void *pointedAddr) throw() :
				m_sptrObjAddr(sptrObjAddr),
				m_pointedAddr(pointedAddr)
			{}

			virtual void Execute(MasterTable &masterTable) override;
		};

		/// <summary>
		/// Message used to inform that the memory address of a new object is to be managed by the GC, 
		/// which means it will handle both the release of memory and object destruction.
		/// </summary>
		class NewObjectMsg : public IMessage
		{
		private:

			void *m_sptrObjectAddr,
				 *m_pointedAddr;
			size_t	m_blockSize;
			FreeMemProc	m_freeMemCallback;

		public:

			NewObjectMsg(void *sptrObjaddr, void *pointedAddr, size_t blockSize, FreeMemProc freeMemCallback) throw() :
				m_sptrObjectAddr(sptrObjaddr),
				m_pointedAddr(pointedAddr),
				m_blockSize(blockSize),
				m_freeMemCallback(freeMemCallback)
			{}

			virtual void Execute(MasterTable &masterTable) override;
		};

		/// <summary>
		/// Message used to inform that the construction of an object has failed, and so its memory must be unregistered
		/// as well as the referer <see cref="sptr"/> object must be updated.
		/// </summary>
		class AbortedObjectMsg : public IMessage
		{
		private:

			void *m_sptrObjAddr;

		public:

			/// <summary>
			/// Initializes a new instance of the <see cref="AbortedObjectMsg"/> class.
			/// </summary>
			/// <param name="sptrObjAddr">The memory address of the <see cref="sptr"/> object
			/// whose referred object experienced failure during construction.</param>
			AbortedObjectMsg(void *sptrObjAddr) throw() :
				m_sptrObjAddr(sptrObjAddr)
			{}

			virtual void Execute(MasterTable &masterTable) override;
		};

		/// <summary>
		/// Message used to inform that a new <see cref="sptr"/> object was created, and so must registered by the GC.
		/// </summary>
		class SptrRegistrationMsg : public IMessage
		{
		private:

			void *m_sptrObjAddr,
				 *m_pointedAddr;

		public:

			SptrRegistrationMsg(void *sptrObjAddr, void *pointedAddr) throw() :
				m_sptrObjAddr(sptrObjAddr),
				m_pointedAddr(pointedAddr)
			{}

			virtual void Execute(MasterTable &masterTable) override;
		};

		/// <summary>
		/// Message used to inform that a <see cref="sptr"/> object was destroyed, and so must be unregistered by the GC.
		/// </summary>
		class SptrUnregistrationMsg : public IMessage
		{
		private:

			void *m_sptrObjAddr;

		public:

			SptrUnregistrationMsg(void *sptrObjAddr) throw() :
				m_sptrObjAddr(sptrObjAddr)
			{}

			virtual void Execute(MasterTable &masterTable) override;
		};


	}// end of memory
}// end of namespace _3fd

#endif // end of header guard