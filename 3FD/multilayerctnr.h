#ifndef CTL_MULTILAYERCTNR_H
#define CTL_MULTILAYERCTNR_H

#include "utils.h"
#include <set>
#include <stack>
#include <vector>
#include <chrono>
#include <algorithm>
#include <functional>
#include <cstdlib>

namespace _3fd
{
	namespace ctl
	{
		using std::find;


		///////////////////////////////////////////
		// MultiLayerCtnr Class
		///////////////////////////////////////////

		template <typename X> 
		class MultiLayerCtnr
		{/* Recursive container of multiple layers. The access to the elements is restricted to searching or sampling. Each 
			instance is a layer which contains elements and cores (references to other instances of this class). Neither the 
			elements nor the cores have to be unique inside the same layer. When trying to access the elements through a value, 
			the elements from the outer layers have the preference. The access to add or remove elements or cores is given only 
			for the outermost layer (actual object). The use of this container is targeted for hierarchical structures where 
			the sons inherit the same elements of their parents (encapsulated as cores), so the memory comsumption is reduced 
			(no data duplication) and the changes made by the parents are automatically available to the sons, once the cores 
			are references to the instances changed by the parents. */
		private:

			////////////////////////////////
			// Non-static members
			////////////////////////////////
		
#ifdef _MSC_VER
			typedef std::multiset<X, 
				std::less<X>, 
				utils::unsafe_pool_allocator<X>> MultiSetOfElements;
		
			typedef std::multiset<MultiLayerCtnr<X> *, 
				std::less<MultiLayerCtnr<X> *>, 
				utils::unsafe_pool_allocator<MultiLayerCtnr<X> *>> MultiSetOfCores;
#else
            typedef std::multiset<X> MultiSetOfElements;

            typedef std::multiset<MultiLayerCtnr<X> *> MultiSetOfCores;
#endif

            MultiSetOfElements m_elements;

			MultiSetOfCores m_cores;

			uint32_t m_pathTrackID;

			bool EmptyImpl(uint32_t pathTrackID);

			/* Reusable recursive algorithm that walks throug the container layers. The function object 
			is a placeholder for a lambda function. The lambda function will define the behavior of the 
			recursion and what is done. */
			bool PerformRecursion(
				uint32_t pathTrackID, 
				const std::function<bool (MultiLayerCtnr<X> &layer)> &goDeeper
			);

			//////////////////////////////
			// Recursion Emulation
			//////////////////////////////

			/* This container was designed to be used in the garbage collector of 3FD. When dealing with 
			tree structures of references, you never know how deep the nodes of the tree can go, and that's 
			why the recursion can be dangerous. It can lead to a stack overflow. In order to avoid recursion, 
			it is replaced by an recursion emulation which uses the following assets: */

			/// <summary>
			/// When emulating recursion, the set of variables inside this structure is used in the iterations of the algorithm.
			/// </summary>
			struct VarSet
			{
				const MultiLayerCtnr<X> &layer;

				typename MultiSetOfCores::const_iterator coresIter;

				bool jump;

				VarSet(const MultiLayerCtnr<X> *mlContainer) throw() : 
					layer(*mlContainer), 
					coresIter(mlContainer->m_cores.cbegin()), 
					jump(false) 
				{
				}
			};

			/* Reusable algorithm that emulates recursion over the container layers. The function object 
			is a placeholder for a lambda function. The lambda function will define the behavior of the 
			recursion and what is done. */
			bool EmulateRecursion(const std::function<bool (VarSet *)> &GoDeeper) const;

		public:

			/// <summary>
			/// Initializes a new instance of the <see cref="MultiLayerCtnr{X}"/> class.
			/// </summary>
			MultiLayerCtnr()
				: m_pathTrackID(0) {}

			/// <summary>
			/// Initializes a new instance of the <see cref="MultiLayerCtnr{X}"/> class using copy semantics.
			/// </summary>
			/// <param name="ob">The object to be copied.</param>
			MultiLayerCtnr(const MultiLayerCtnr &ob)
			{
				*this = ob;
			}

			/// <summary>
			/// Initializes a new instance of the <see cref="MultiLayerCtnr{X}"/> class using move semantics.
			/// </summary>
			/// <param name="ob">The object whose resrouces will be stolen.</param>
			MultiLayerCtnr(MultiLayerCtnr &&ob)
			{
				*this = std::move(ob);
			}

			/// <summary>
			/// Assignment operator using copy semantics.
			/// </summary>
			/// <param name="ob">The object to assign.</param>
			/// <returns>A reference to the caller</returns>
			MultiLayerCtnr &operator =(const MultiLayerCtnr &ob)
			{
				if(&ob != this)
				{		
					// Copy containers:
					m_elements = ob.m_elements;
					m_cores = ob.m_cores;
					m_pathTrackID = ob.m_pathTrackID;
				}
			}

			/// <summary>
			/// Assignment operator using move semantics.
			/// </summary>
			/// <param name="ob">The object to move-assign.</param>
			/// <returns>A reference to the caller</returns>
			MultiLayerCtnr &operator =(MultiLayerCtnr &&ob)
			{
				if(&ob != this)
				{
					// Move containers:
					m_cores = std::move(ob.m_cores);
					m_elements = std::move(ob.m_elements);
					m_pathTrackID = ob.m_pathTrackID;
				}
			}

			void AddToLayer(const X &element)
			{
				m_elements.insert(element);
			}

			void AddToLayer(X &&element)
			{
				m_elements.insert(std::move(element));
			}

			void AddToLayer(MultiLayerCtnr &core)
			{
				m_cores.insert(&core);
			}

			void RemoveFromLayer(const X &element)
			{
				auto iter = m_elements.find(element);
				_ASSERTE(iter != m_elements.cend()); // Trying to remove a non existent element from the container
				m_elements.erase(iter);
			}

			void RemoveFromLayer(MultiLayerCtnr &core)
			{
				auto iter = m_cores.find(&core);
				_ASSERTE(iter != m_cores.end()); // Trying to remove a non existent core from the container
				m_cores.erase(iter);
			}

			void ClearLayer() throw()
			{
				m_elements.clear();
				m_cores.clear();
			}

			std::set<X> GetDistinctElements();

			bool Find(const X &element);

			bool Empty();
		};
	

		////////////////////////////
		// More Implementation
		////////////////////////////

		/// <summary>
		/// Generates a unique track identifier.
		/// </summary>
		/// <param name="somePointer">Some pointer to a memory address.</param>
		/// <returns>A randomly generated unique track ID.</returns>
		static uint32_t GenerateTrackID(const void *somePointer)
		{
			using namespace std::chrono;
            static const long long maskLower32bits = (1UL << 32) - 1;
			
			// Generate a distinct ID to identify the path the algorithm will take:
			const uint32_t pathTrackID = 
				(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() & maskLower32bits) // the lower 32 bits of time
				^ reinterpret_cast<long long> (somePointer) // XOR with some memory address
				^ rand(); // XOR with a random integer

			return pathTrackID;
		}

		template <typename X> 
		bool MultiLayerCtnr<X>::Find(const X &element)
		{
			auto wasNotFoundInLayerElements = 
				[element] (MultiLayerCtnr<X> &layer) -> bool
				{
					return layer.m_elements.find(element) == layer.m_elements.cend();
				};

			return PerformRecursion(GenerateTrackID(this), wasNotFoundInLayerElements);
		}

		template <typename X> 
		std::set<X> MultiLayerCtnr<X>::GetDistinctElements()
		{
			std::set<X> allElements;

			auto getAllLayerElements = 
				[&allElements] (MultiLayerCtnr<X> &layer) throw() -> bool
				{
					for(auto &element : layer.m_elements)
						allElements.insert(element);
			
					return true;
				};

			PerformRecursion(GenerateTrackID(this), getAllLayerElements);

			return std::move(allElements);
		}

		/// <summary>
		/// Implementation of the recursive body of 'Empty'.
		/// </summary>
		/// <returns>'true' if no element was found, otherwise, 'false'.</returns>
		template <typename X> 
		bool MultiLayerCtnr<X>::EmptyImpl(uint32_t pathTrackID)
		{
			if(m_elements.empty() == false) // elements found
				return false;
			else // no elements found
			{
				if(m_cores.empty() == false)
				{
					// Go through the inner cores to see if they are empty:
					for(auto core : m_cores)
					{
						if(m_pathTrackID != pathTrackID) // Not in a cycle:
						{
							m_pathTrackID = pathTrackID; // mark core to avoid cyclic recursion

							if( core->EmptyImpl(pathTrackID) ) // this core is empty:
								continue; // try another core
							else // this core is not empty:
								return false; // stop recursion
						}
						else // Path is a cycle:
							return true;
					}

					// Finished scanning cores. All are empty.
					return true;
				}
				else // no cores in the current layer
					return true;
			}
		}

		/// <summary>
		/// Evaluates whether the container is empty of elements or not.
		/// Truly recursive.
		/// Not thread-safe.
		/// </summary>
		/// <returns>'true' if no element was found, otherwise, 'false'.</returns>
		template <typename X> 
		bool MultiLayerCtnr<X>::Empty()
		{
			return EmptyImpl(GenerateTrackID(this));
		}
		
		/// <summary>
		/// Performs a recursive procedure controlled by a predicate.
		/// </summary>
		/// <param name="goDeeper">
		/// A predicate which operates over the elements (but not the cores) of the current layer.
		/// When the predicate returns 'true', the execution go to the inner layers (cores inside the inner layer).
		/// It controls how far the recursion goes.
		/// </param>
		/// <returns>
		/// 'false' if the end of the container has been reached.
		/// 'true' if the recursion was interrupted before reaching the end.
		/// </returns>
		template <typename X> 
		bool MultiLayerCtnr<X>::PerformRecursion(
			uint32_t pathTrackID, 
			const std::function<bool (MultiLayerCtnr<X> &layer)> &goDeeper
		)
		{
			m_pathTrackID = pathTrackID;

			// Test if it should go deeper into the recursion (get into the inner layers)
			if(goDeeper(*this))
			{
				for(auto &core : m_cores)
				{
					if(core->m_pathTrackID != pathTrackID) // This test prevents the recursion to enter in a cycle
					{
						if (core->PerformRecursion(pathTrackID, goDeeper) == false)
							continue;
						else
							return true;
					}
				}
				
				return false; // end of the container has been reached
			}
			else
				return true; // recursion interrupted
		}

		/// <summary>
		/// Emulates a recursive procedure controlled by a predicate.
		/// </summary>
		/// <param name="GoDeeper">
		/// A predicate which operates over the elements (but not the cores) of the current layer.
		/// When the predicate returns 'true', the execution go to the inner layers (cores inside the inner layer).
		/// It controls how far the recursion goes.
		/// </param>
		/// <returns>
		/// 'false' if the end of the container has been reached.
		/// 'true' if the recursion was interrupted before reaching the end.
		/// </returns>
		template <typename X> 
		bool MultiLayerCtnr<X>::EmulateRecursion(const std::function<bool (VarSet *)> &GoDeeper) const
		{
			std::stack<VarSet *> activationStates; // The state of the emulated activations will be stored in a STL stack object

			/* In a given moment of this method execution, there is a path of cores that make the way until the 
			current state. Those cores are stored in the following STL set object. So when a cyclic inheritance 
			takes place, it is possible to detect because the cycle closes when a core hosts a core of a past 
			state which is stored in the set.*/
			std::set<const MultiLayerCtnr<X> *> coresTrack;

			coresTrack.insert(this); // The first core is the calling object itself

			VarSet *curState = new VarSet (this); // The current state starts in the current layer (instance)

			/* Test if it should go deeper into the recursion (get into the inner layers).
			If the execution has rolled back to a higher layer (curState->jump == true), 
			it skips the test and jumps directly into the loop */
			while( curState->jump || GoDeeper(curState) )
			{
				// Switch the state to a core inside the layer, if there is one:
				if( curState->coresIter != curState->layer.m_cores.cend() )
				{				
					curState->jump = true; // If the execution rolls back to this state, jump the first loop test

					auto core = *curState->coresIter;

					if( coresTrack.find(core) == coresTrack.end() ) // This test prevents the recursion to enter in a cycle
					{	
						activationStates.push(curState);
						coresTrack.insert(core);
						curState = new VarSet (core); // Create the new state (will be used in the next iteration)
					}
					else // If there is a cycle, try another path:
						++(curState->coresIter);
				}
				else
				{// If there is no remaining cores inside the layer, roll back to the previous state:
					delete curState;

					if(activationStates.empty() == false)
					{
						curState = activationStates.top();
						activationStates.pop();
						coresTrack.erase(*curState->coresIter);
						++(curState->coresIter);
					}
					else // It reached the end of the container: get out of the loop returning false
						return false;
				}
			}

			// "Stack unwinding" phase:

			delete curState;

			// Clear the remaining activationStates:
			while( !activationStates.empty() )
			{
				delete activationStates.top();
				activationStates.pop();
			}

			return true;
		}

	}// end of namespace ctl
}// end of namespace _3fd

#endif // header guard
