#ifndef GC_MEMADDRESS_H // header guard
#define GC_MEMADDRESS_H

#include <cstdint>

namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// Holds a single memory address which can be flagged using less significant bits available.
		/// </summary>
		class MemAddress
		{
		private:

			mutable void *m_address;

#if defined(_M_X64) || defined(__amd64__)
			static const uintptr_t mask = 0xfffffffffffffffe;
#else
			static const uintptr_t mask = 0xfffffffe;
#endif
		public:

			/// <summary>
			/// Initializes a new instance of the <see cref="MemAddress"/> class.
			/// </summary>
			/// <param name="address">The memory address.</param>
			explicit MemAddress(void *address)
				: m_address(address) {}

			/// <summary>
			/// Gets the stored memory address.
			/// </summary>
			/// <returns>The memory address, without the encoded flag.</returns>
			void *Get() const
			{
				return reinterpret_cast <void *> (reinterpret_cast<uintptr_t> (m_address) & mask);
			}

			/// <summary>
			/// Sets a bit flag to mark the memory address.
			/// </summary>
			/// <param name="on">if set to <c>true</c>, activates the less significant bit in the address.</param>
			void Mark(bool on) const
			{
				if (on)
					m_address = reinterpret_cast <void *> (reinterpret_cast<uintptr_t> (m_address) | 1);
				else
					m_address = reinterpret_cast <void *> (reinterpret_cast<uintptr_t> (m_address) & mask);
			}

			/// <summary>
			/// Determines whether the memory address is marked.
			/// </summary>
			/// <returns>Whether a bit flag was activated to mark the memory address.</returns>
			bool isMarked() const
			{
				return static_cast<bool> (reinterpret_cast<uintptr_t> (m_address)& 1);
			}

			/// <summary>
			/// Equality operator.
			/// </summary>
			/// <param name="ob">The object to compare with.</param>
			/// <returns><c>true</c> if both object are equivalent, which means they have same address and mark, otherwise, <c>false</c>.</returns>
			bool operator ==(const MemAddress &ob)
			{
				return m_address == ob.m_address;
			}
		};

		/// <summary>
		/// Base class for <see cref="MemBlock"/>.
		/// Its only purpose is to ease searching a std::set{MemBlock *}.
		/// </summary>
		class MemAddrContainer
		{
		private:

			MemAddress m_memAddr;

		public:

			/// <summary>
			/// Initializes a new instance of the <see cref="MemAddrContainer"/> class.
			/// </summary>
			/// <param name="memAddress">The memory address to store.</param>
			MemAddrContainer(void *memAddress)
				: m_memAddr(memAddress) {}

			MemAddress &memoryAddress() { return m_memAddr; }

			const MemAddress &memoryAddress() const { return m_memAddr; }
		};

		/// <summary>
		/// Implementation of functor for std::set{MemAddrContainer} that takes into consideration 
		/// the address of the represented memory pieces, rather than the addresses of the actual
		/// <see cref="MemAddrContainer"/> objects.
		/// </summary>
		struct LessOperOnMemBlockRepAddr
		{
			bool operator()(MemAddrContainer *left, MemAddrContainer *right) const
			{
				return left->memoryAddress().Get()
					< right->memoryAddress().Get();
			}
		};

	}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
