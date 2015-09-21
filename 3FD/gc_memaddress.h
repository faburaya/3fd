#ifndef GC_MEMADDRESS_H // header guard
#define GC_MEMADDRESS_H

#include <cstdint>

namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// Holds a single memory address which can be flagged using the 2 less significant bits.
		/// The availability of such bits must be ensured allocating aligned memory.
		/// </summary>
		class MemAddress
		{
		private:

			mutable void *m_address;

#if defined(_M_X64) || defined(__amd64__)
			static const uintptr_t mask = 0xfffffffffffffffc;
#else
			static const uintptr_t mask = 0xfffffffc;
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
			/// Gets the stored memory address.
			/// </summary>
			/// <returns>The memory address, WITH the encoded flag.</returns>
			void *GetEncoded() const
			{
				return reinterpret_cast <void *> (reinterpret_cast<uintptr_t> (m_address));
			}

			/// <summary>
			/// Sets the less significant bit in the memory address.
			/// </summary>
			/// <param name="on">
			/// if set to <c>true</c>, activates the less significant bit.
			/// </param>
			void SetBit0(bool on) const
			{
				if (on)
				{
					m_address = reinterpret_cast <void *> (
						reinterpret_cast<uintptr_t> (m_address) | uintptr_t(1)
					);
				}
				else
				{
					m_address = reinterpret_cast <void *> (
						reinterpret_cast<uintptr_t> (m_address) & (uintptr_t(2) & mask)
					);
				}
			}

			/// <summary>
			/// Determines whether the memory address has the less significant bit set.
			/// </summary>
			/// <returns>
			/// Whether the memory address is marked with the less significant bit activated.
			/// </returns>
			bool GetBit0() const
			{
				return (reinterpret_cast<uintptr_t> (m_address) & uintptr_t(1)) != 0;
			}

			/// <summary>
			/// Sets the second less significant bit in the memory address.
			/// </summary>
			/// <param name="on">
			/// if set to <c>true</c>, activates the second less significant bit.
			/// </param>
			void SetBit1(bool on) const
			{
				if (on)
				{
					m_address = reinterpret_cast <void *> (
						reinterpret_cast<uintptr_t> (m_address) | uintptr_t(2)
					);
				}
				else
				{
					m_address = reinterpret_cast <void *> (
						reinterpret_cast<uintptr_t> (m_address)& (uintptr_t(1) & mask)
					);
				}
			}

			/// <summary>
			/// Determines whether the memory address has the second less significant bit set.
			/// </summary>
			/// <returns>
			/// Whether the memory address is marked with the second less significant bit activated.
			/// </returns>
			bool GetBit1() const
			{
				return (reinterpret_cast<uintptr_t> (m_address) & uintptr_t(2)) != 0;
			}

			/// <summary>
			/// Equality operator.
			/// </summary>
			/// <param name="ob">The object to compare with.</param>
			/// <returns>
			/// <c>true</c> if both object are equivalent,
			/// which means they have same address and mark,
			/// otherwise, <c>false</c>.
			/// </returns>
			bool operator ==(const MemAddress &ob)
			{
				return m_address == ob.m_address;
			}
		};

	}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
