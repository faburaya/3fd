#include "stdafx.h"
#include "utils_arrayofbits.h"
#include "exceptions.h"

#include <cstdlib>

namespace _3fd
{
	namespace utils
	{
		/// <summary>
		/// Gets the room in words that fits a given amount of bits.
		/// </summary>
		/// <param name="nBits">The amount of bits.</param>
		/// <returns>The minimum room in words (32/64 bits) necessary to store the given amount of bits.</returns>
		static size_t GetRoomInWordsFor(size_t nBits)
		{
			const auto bitsInWord = 8 * sizeof(void *);
			unsigned int correction = (nBits % bitsInWord > 0) ? 1 : 0;
			return correction + nBits / bitsInWord;
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="ArrayOfBits"/> class.
		/// </summary>
		/// <param name="nBits">The number of bits.</param>
		/// <param name="val">if set to <c>true</c>, initialize all bits in the array as activated, otherwise, deactivated.</param>
		ArrayOfBits::ArrayOfBits(size_t nBits, bool val) : 
			m_nBits(nBits),
			m_data(nullptr)
		{
			if (val)
			{
				const auto nBytes = GetRoomInWordsFor(nBits) * sizeof(void *);
				m_data = (uintptr_t *)malloc(nBytes);

				if (m_data == nullptr)
					throw core::AppException<std::runtime_error>("Failed to allocate memory for array of bits", "std::malloc");

				memset(m_data, 0xff, nBytes);
				m_activatedBitsCount = nBytes;
			}
			else
			{
				m_data = (uintptr_t *)calloc(GetRoomInWordsFor(nBits), sizeof(void *));

				if (m_data == nullptr)
					throw core::AppException<std::runtime_error>("Failed to allocate memory for array of bits", "std::calloc");

				m_activatedBitsCount = 0;
			}
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="ArrayOfBits"/> class using move semantics.
		/// </summary>
		/// <param name="ob">The object whose resources will be stolen.</param>
		ArrayOfBits::ArrayOfBits(ArrayOfBits &&ob) : 
			m_nBits(ob.m_nBits),
			m_data(ob.m_data),
			m_activatedBitsCount(ob.m_activatedBitsCount)
		{
			ob.m_data = 0;
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="ArrayOfBits"/> class.
		/// </summary>
		ArrayOfBits::~ArrayOfBits()
		{
			if (m_data != nullptr)
				free(m_data);
		}

		/// <summary>
		/// Gets the memory address of the word containing a given bit in the array.
		/// </summary>
		/// <param name="bitIdx">The index of the desired bit.</param>
		/// <param name="bitInPos">The position (0 based) of the bit inside the word, increasing from the least to the most significant bit.</param>
		/// <returns>
		/// A pointer to the word inside the array that contains the bit in the specified position.
		/// </returns>
		uintptr_t * ArrayOfBits::GetWord(size_t bitIdx, uint32_t &bitInPos) const
		{
			_ASSERTE(bitIdx < m_nBits); // out-of-bound memory access violation!
			const auto bitsInWord = 8 * sizeof(void *);
			auto wordIdx = bitIdx / bitsInWord;
			bitInPos = (wordIdx - 1) - (bitIdx - wordIdx * bitsInWord);
			return m_data + wordIdx;
		}

		/// <summary>
		/// Gets the state of a specified bit.
		/// </summary>
		/// <param name="bitIdx">The index of the bit.</param>
		/// <returns><c>true</c> when the specified bit is activated, otherwise, <c>false</c>.</returns>
		bool ArrayOfBits::operator[](size_t bitIdx) const
		{
			uint32_t bitPos;
			uintptr_t *ptr = GetWord(bitIdx, bitPos);
			uintptr_t mask = 1 << bitPos;
			return (*ptr & mask) != 0;
		}

		/// <summary>
		/// Gets the bit index of the most significant activated bit inside the given word.
		/// </summary>
		/// <param name="word">The given word.</param>
		/// <returns>The position (0 based) of the bit, increasing along with the data address.</returns>
		/// <remarks>This algorithm works for little-endian byte encoding only!</remarks>
		static uint32_t GetIdxHiActBitInWord(uintptr_t word)
		{
			_ASSERTE(word != 0); // it is assumed there is an activated bit

			auto movingBit = 1 + (~(uintptr_t)0 >> 1); // start from the most significant bit
			uint32_t pos(0);
			while (word & movingBit == 0)
			{
				++pos;
				movingBit >>= 1;
			}

			return pos;
		}

		/// <summary>
		/// Finds the first activated bit in the array.
		/// </summary>
		/// <returns>
		/// The index where the first activated bit has been found. If none has been found, returns the same as 'Size()'.
		/// </returns>
		size_t ArrayOfBits::FindFirstActivated() const
		{
			if (m_activatedBitsCount > 0)
			{
				const auto bitsInWord = 8 * sizeof(void *);
				const auto last = m_data + m_nBits / bitsInWord;

				auto iter = m_data;
				while (iter < last && *iter == 0)
					++iter;

				if (iter < last)
					return (iter - m_data) + GetIdxHiActBitInWord(*iter);

				/* The last word might contain unused bits that must be ignored. The bits to take into
				account are the ones which contribute above a given value, as calculated below: */
				auto unusedBits = (1 << (bitsInWord - (Size() % bitsInWord))) - 1;
				if (*iter > unusedBits)
					return (iter - m_data) + GetIdxHiActBitInWord(*iter);
			}

			return Size();
		}

		/// <summary>
		/// Finds the first deactivated bit in the array.
		/// </summary>
		/// <returns>The index where the first deactivated bit has been found.</returns>
		size_t ArrayOfBits::FindFirstDeactivated() const
		{
			if (m_activatedBitsCount < m_nBits)
			{
				const auto bitsInWord = 8 * sizeof(void *);
				const auto max = ~(uintptr_t)0;
				const auto last = m_data + m_nBits / bitsInWord;

				auto iter = m_data;
				while (iter < last && *iter == max)
					++iter;

				if (iter < last)
					return (iter - m_data) + GetIdxHiActBitInWord(~*iter);

				/* The last word might contain unused bits that must be ignored. The bits to take into
				account are the ones which contribute above a given value, as calculated below: */
				auto usedBits = ~((1 << (bitsInWord - (Size() % bitsInWord))) - 1);
				if (*iter < usedBits)
					return (iter - m_data) + GetIdxHiActBitInWord(~*iter);
			}

			return Size();
		}

		/// <summary>
		/// Finds the last activated bit in the array.
		/// </summary>
		/// <returns>
		/// The index where the last activated bit has been found. If none has been found, returns the same as 'Size()'.
		/// </returns>
		size_t ArrayOfBits::FindLastActivated() const
		{
			if (m_activatedBitsCount > 0)
			{
				const auto bitsInWord = 8 * sizeof(void *);
				const auto last = m_data + m_nBits / bitsInWord;

				/* Start by the last word, but it might contain unused bits that must be ignored.
				The bits to take into account are the ones which contribute above a given value,
				as calculated below : */
				auto unusedBits = (1 << (bitsInWord - (Size() % bitsInWord))) - 1;
				if (*last >= unusedBits)
					return (last - m_data) + GetIdxHiActBitInWord(*last);

				auto iter = last - 1;
				while (iter >= m_data)
				{
					if (*iter == 0)
						--iter;
					else
						return (iter - m_data) + GetIdxHiActBitInWord(*iter);
				}
			}

			return Size();
		}

		/// <summary>
		/// Finds the last activated bit in the array.
		/// </summary>
		/// <returns>The index where the last deactivated bit has been found.</returns>
		size_t ArrayOfBits::FindLastDeactivated() const
		{
			if (m_activatedBitsCount < m_nBits)
			{
				const auto bitsInWord = 8 * sizeof(void *);
				const auto last = m_data + m_nBits / bitsInWord;

				/* Start by the last word, but it might contain unused bits that must be ignored.
				The bits to take into account are the ones which contribute above a given value,
				as calculated below : */
				auto usedBits = ~((1 << (bitsInWord - (Size() % bitsInWord))) - 1);
				if (*last < usedBits)
					return (last - m_data) + GetIdxHiActBitInWord(~*last);

				const auto max = ~(uintptr_t)0;
				auto iter = last - 1;
				while (iter >= m_data)
				{
					if (*iter == max)
						--iter;
					else
						return (iter - m_data) + GetIdxHiActBitInWord(~*iter);
				}
			}

			return Size();
		}

		/// <summary>
		/// Activates a given bit.
		/// </summary>
		/// <param name="bitIdx">The bit index.</param>
		void ArrayOfBits::Activate(size_t bitIdx)
		{
			uint32_t bitPos;
			uintptr_t *ptr = GetWord(bitIdx, bitPos);
			auto mask = static_cast<uintptr_t> (1 << bitPos);

			if (*ptr & mask == 0)
			{
				*ptr |= mask;
				++m_activatedBitsCount;
			}
		}

		/// <summary>
		/// Deactivates a given bit.
		/// </summary>
		/// <param name="bitIdx">The bit index.</param>
		void ArrayOfBits::Deactivate(size_t bitIdx)
		{
			uint32_t bitPos;
			uintptr_t *ptr = GetWord(bitIdx, bitPos);
			auto mask = static_cast<uintptr_t> (1 << bitPos);

			if (*ptr & mask != 0)
			{
				*ptr &= mask;
				--m_activatedBitsCount;
			}
		}

	}// end of namespace utils
}// end of namespace _3fd
