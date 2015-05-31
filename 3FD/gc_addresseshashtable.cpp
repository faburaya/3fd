#include "stdafx.h"
#include "gc_addresseshashtable.h"
#include "configuration.h"

namespace _3fd
{
	namespace memory
	{
		using core::AppConfig;

		/// <summary>
		/// Initializes a new instance of the <see cref="AddressesHashTable"/> class.
		/// </summary>
		AddressesHashTable::AddressesHashTable() :
			m_elementsCount(0),
			m_outHashSizeInBits(0)
		{}

		/// <summary>
		/// Hashes a key using the FNV1a algorithm modified for outputs smaller than 16 bits.
		/// </summary>
		/// <param name="key">The key.</param>
		/// <returns>The hashed key.</returns>
		size_t AddressesHashTable::Hash(void *key)
		{
			uint32_t hash = 2166136261; // use the FNV offset basis for 32 bits hashes

			for (int bitsToShift = ((sizeof key) - 1) * 8;
				 bitsToShift >= 0;
				 bitsToShift -= 8)
			{
				auto octet = (reinterpret_cast<uintptr_t> (key) >> bitsToShift) & 255;
				hash ^= octet;
				hash *= 16777619; // use the FNV prime for 32 bits hashes
			}

			return hash;
		}

		/// <summary>
		/// XOR-fold's a hash to adapt to the bucket array size.
		/// </summary>
		/// <param name="hash">The hash.</param>
		/// <param name="outHashSizeInBits">The size of the returned hash in bits.</param>
		/// <returns>The xor-folded hash.</returns>
		size_t AddressesHashTable::XorFold(size_t hash, uint32_t outHashSizeInBits)
		{
			const uint32_t maskForLowerBits = (1UL << outHashSizeInBits) - 1;
			return ((hash >> outHashSizeInBits) ^ hash) & maskForLowerBits;
		}

		/// <summary>
		/// Expands the hash table to twice its size.
		/// </summary>
		void AddressesHashTable::ExpandTable()
		{
			if (m_bucketArray.empty() == false)
			{
				++m_outHashSizeInBits;
				std::vector<Element> newArray(1 << m_outHashSizeInBits);

				// Rehash the table to the new array:
				for (auto &element : m_bucketArray)
				{
					if (element.GetSptrObjectAddr() != nullptr)
					{
						auto hashedKey = Hash(element.GetSptrObjectAddr());
						auto idx = XorFold(hashedKey, m_outHashSizeInBits);

						if (newArray[idx].GetSptrObjectAddr() == nullptr)
							newArray[idx] = element;
						else
						{// Linear probe:
							do
							{
								if (++idx < newArray.size())
									continue;
								else
									idx = 0;
							} while (newArray[idx].GetSptrObjectAddr() != nullptr);

							newArray[idx] = element;
						}
					}
				}

				m_bucketArray.swap(newArray);
			}
			else
			{// Allocate the bucket array for the first time:
				m_outHashSizeInBits = AppConfig::GetSettings().framework.gc.sptrObjectsHashTable.initialSizeLog2;
				m_bucketArray.resize(1 << m_outHashSizeInBits);
			}
		}

		/// <summary>
		/// Shrinks the hash table to half its size.
		/// </summary>
		void AddressesHashTable::ShrinkTable()
		{
			--m_outHashSizeInBits;
			const auto newSize = 1 << m_outHashSizeInBits;
			_ASSERTE(newSize >= m_elementsCount); // Can only shrink the table if the new table can fit all currently stored elements

			std::vector<Element> newArray(newSize);

			// Rehash the table to the new array:
			for (auto &element : m_bucketArray)
			{
				if (element.GetSptrObjectAddr() == nullptr)
					continue;
				else
				{
					auto hashedKey = Hash(element.GetSptrObjectAddr());
					auto idx = XorFold(hashedKey, m_outHashSizeInBits);

					if (newArray[idx].GetSptrObjectAddr() == nullptr)
						newArray[idx] = element;
					else
					{// Linear probe:
						do
						{
							if (++idx < newArray.size())
								continue;
							else
								idx = 0;
						} while (newArray[idx].GetSptrObjectAddr() != nullptr);

						newArray[idx] = element;
					}
				}
			}

			m_bucketArray.swap(newArray);
		}

		/// <summary>
		/// Inserts a new entry in the hash table, placed according the memory address of the <see cref="sptr" /> object.
		/// </summary>
		/// <param name="sptrObjectAddr">The <see cref="sptr" /> object address.</param>
		/// <param name="pointedAddr">The address pointed by the <see cref="sptr" /> object.</param>
		/// <param name="container">The container memory block.</param>
		/// <returns>A view to the inserted element.</returns>
		AddressesHashTable::Element &
		AddressesHashTable::Insert(void *sptrObjectAddr, void *pointedAddr, MemAddrContainer *container)
		{
			if (CalculateLoadFactor() > AppConfig::GetSettings().framework.gc.sptrObjectsHashTable.loadFactorThreshold
				|| m_bucketArray.empty())
			{
				ExpandTable();
			}

			auto hashedKey = Hash(sptrObjectAddr);
			auto idx = XorFold(hashedKey, m_outHashSizeInBits);

			/* The code below could be simplified, but it will be kept this way so as to prevent
			misprediction of instructions. However, that should not be a concern if the compiler
			in use will apply PGO.*/

			if (m_bucketArray[idx].GetSptrObjectAddr() == nullptr)
			{
				++m_elementsCount;
				return m_bucketArray[idx] = Element(sptrObjectAddr, pointedAddr, container);
			}
			else // Linear probe:
			{
				auto &element = m_bucketArray[idx];
				auto displacedElement = element;

				// Place the new element in the first hashed index
				element = Element(sptrObjectAddr, pointedAddr, container);
				++m_elementsCount;

				// And move the displaced element to the first vacant position:
				do
				{
					if (++idx < m_bucketArray.size())
						continue;
					else
						idx = 0;
				} while (m_bucketArray[idx].GetSptrObjectAddr() != nullptr);

				m_bucketArray[idx] = displacedElement;

				// Return a reference inserted element:
				return element;
			}
		}

		/// <summary>
		/// Looks up for the specified <see cref="sptr" /> object address.
		/// </summary>
		/// <param name="sptrObjectAddr">The <see cref="sptr" /> object address.</param>
		/// <returns>The <see cref="Element" /> object corresponding to the <see cref="sptr" /> object address.</returns>
		AddressesHashTable::Element &
		AddressesHashTable::Lookup(void *sptrObjectAddr)
		{
			auto hashedKey = Hash(sptrObjectAddr);
			auto idx = XorFold(hashedKey, m_outHashSizeInBits);

			if (m_bucketArray[idx].GetSptrObjectAddr() == sptrObjectAddr)
				return m_bucketArray[idx];
			else
			{// Linear probe:
				do
				{
					if (++idx < m_bucketArray.size())
						continue;
					else
						idx = 0;
				} while (m_bucketArray[idx].GetSptrObjectAddr() != sptrObjectAddr);

				return m_bucketArray[idx];
			}
		}

		/// <summary>
		/// Removes the element corresponding to a <see cref="sptr" /> object address.
		/// </summary>
		/// <param name="sptrObjectAddr">The specified <see cref="sptr" /> object address.</param>
		void AddressesHashTable::Remove(void *sptrObjectAddr)
		{
			Lookup(sptrObjectAddr) = Element();
			--m_elementsCount;

			if (m_outHashSizeInBits > AppConfig::GetSettings().framework.gc.sptrObjectsHashTable.initialSizeLog2
				&& CalculateLoadFactor() < AppConfig::GetSettings().framework.gc.sptrObjectsHashTable.loadFactorThreshold / 3)
			{
				ShrinkTable();
			}
		}

	}// end of namespace memory
}// end of namespace _3fd