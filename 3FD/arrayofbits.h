#ifndef UTILS_ARRAYOFBITS_H // header guard
#define UTILS_ARRAYOFBITS_H

#include "base.h"
#include "exceptions.h"

#include <cstdint>

namespace _3fd
{
	namespace utils
	{
		/// <summary>
		/// A fixed size array of booleans stored as a single bit.
		/// </summary>
		class ArrayOfBits : notcopiable
		{
		private:

			const size_t m_nBits;

			uintptr_t *m_data;
			size_t m_activatedBitsCount;

			uintptr_t *GetWord(size_t bitIdx, uint32_t &bitInPos) const;

		public:

			ArrayOfBits(size_t nBits, bool val);

			ArrayOfBits(ArrayOfBits &&ob);

			~ArrayOfBits();

			/// <summary>
			/// Gets the amount of bits the array was set to store.
			/// </summary>
			/// <returns>How many bits the array was set to store upon construction.</returns>
			size_t Size() const { return m_nBits; }

			/// <summary>
			/// Gets how many bits in the array are currently activated.
			/// </summary>
			/// <returns>The amount of activated bits in the array.</returns>
			size_t GetActivatedCount() const { return m_activatedBitsCount; }

			/// <summary>
			/// Determines whether there is any activated bit in the array.
			/// </summary>
			/// <returns>
			/// <c>true</c> if there is at least one activated bit in the array, otherwise, <c>false</c>.
			/// </returns>
			bool IsAnyActivated() const { return m_activatedBitsCount > 0; }

			bool operator[](size_t bitIdx) const;

			size_t FindFirstActivated() const;

			size_t FindFirstDeactivated() const;

			size_t FindLastActivated() const;

			size_t FindLastDeactivated() const;

			void Activate(size_t bitIdx);

			void Deactivate(size_t bitIdx);
		};

	}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
