#ifndef GC_ADDRESSESHASHTABLE_H // header guard
#define GC_ADDRESSESHASHTABLE_H

#include "base.h"
#include "gc_vertex.h"
#include <vector>
#include <cstdint>

namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// This class uses hash table data structure (with open addressing and linear probing) to store information about 
		/// the <see cref="sptr" /> objects managed by the GC. It was not converted to a template because it was designed 
		/// very specifically (optimized) for its job. The implementation could be more "OOP/C++ like", but the concern here 
		/// is to save memory. If you find yourself wishing to change its model to make it more OOP compliant, remember it 
		/// was designed that way so as to save something around 8 or 16 bytes per element added to the table.
		/// </summary>
		class AddressesHashTable : notcopiable
		{
		public:

			/// <summary>
			/// This structure is the actual bucket element
			/// </summary>
			class Element
			{
			private:

				void *m_sptrObjectAddr; // This is the unique key (the memory address of the sptr object)
				Vertex *m_pointedMemBlock; // This is a value (what the sptr points to)
				Vertex *m_containerMemBlock; // this is a value (where the sptr lives)

			public:

				/// <summary>
				/// Initializes a new instance of the <see cref="Element"/> class.
				/// </summary>
				Element() :
					m_sptrObjectAddr(nullptr),
					m_pointedMemBlock(nullptr),
					m_containerMemBlock(nullptr)
				{}

				/// <summary>
				/// Constructor for the <see cref="Element" /> objects allocated on the stack.
				/// </summary>
				/// <param name="sptrObjectAddr">The <see cref="sptr" /> object address.</param>
				/// <param name="pointedMemBlock">
				/// The vertex representing the memory block pointed by this <see cref="sptr" /> object.
				/// </param>
				/// <param name="containerMemBlock">
				/// The vertex representing the memory block that contains this <see cref="sptr" /> object.
				/// </param>
				Element(void *sptrObjectAddr, Vertex *pointedMemBlock, Vertex *containerMemBlock) :
					m_sptrObjectAddr(sptrObjectAddr),
					m_pointedMemBlock(pointedMemBlock),
					m_containerMemBlock(containerMemBlock)
				{}

				// Get the address of the sptr object this element represents
				void *GetSptrObjectAddr() const { return m_sptrObjectAddr; }

				// Get the address of the memory block this pointer refers to
				void *GetPointedAddr() const
				{
					return m_pointedMemBlock != nullptr
						? m_pointedMemBlock->GetMemoryAddress().Get()
						: nullptr;
				}

				// Get the vertex representing the memory block this pointer refers to
				Vertex *GetPointedMemBlock() const { return m_pointedMemBlock; }

				// Set the vertex representing the memory block this pointer refers to
				void SetPointedMemBlock(Vertex *pointedMemBlock) { m_pointedMemBlock = pointedMemBlock; }

				// Get the vertex for the memory block that contains the sptr object this element represents
				Vertex *GetContainerMemBlock() const { return m_containerMemBlock; }

				// Determines whether the sptr object this element represents is a root vertex
				bool IsRoot() const { return m_containerMemBlock == nullptr; }
			};

		private:

			std::vector<Element> m_bucketArray;
			size_t m_elementsCount;
			uint32_t m_outHashSizeInBits;

			/// <summary>
			/// Calculates the load factor.
			/// </summary>
			/// <returns>The current load factor of the table.</returns>
			float CalculateLoadFactor() const
			{
				if (m_bucketArray.empty() == false)
					return ((float)m_elementsCount) / m_bucketArray.size();
				else
					return 0.0;
			}

			static size_t Hash(void *key);
			static size_t XorFold(size_t hash, uint32_t outHashSizeInBits);

			void ExpandTable();
			void ShrinkTable();

		public:

			AddressesHashTable();

			Element &Insert(void *sptrObjectAddr,
							Vertex *pointedMemBlock,
							Vertex *containerMemBlock);

			Element &Lookup(void *sptrObjectAddr);

			void Remove(Element &element);

			void Remove(void *sptrObjectAddr);
		};

	}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
