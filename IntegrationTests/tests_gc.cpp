#include "stdafx.h"
#include "runtime.h"
#include "sptr.h"
#include <map>
#include <list>
#include <array>
#include <chrono>
#include <thread>
#include <future>
#include <random>
#include <iostream>

namespace _3fd
{
namespace integration_tests
{
	using std::generate;
	using std::for_each;

	using namespace _3fd::core;
	using memory::sptr;
	using memory::const_sptr;

	void HandleException();

	/// <summary>
	/// Hold some resources 
	/// </summary>
	class ResourceHolder
	{
	private:

		std::vector<int> m_resource;

	public:

		ResourceHolder(bool fail = false)
		{
			m_resource.reserve(3);
			m_resource.push_back(6);
			m_resource.push_back(9);
			m_resource.push_back(6);

			if(fail)
				throw AppException<std::runtime_error>("Generic failure during construction.");
		}

		~ResourceHolder()
		{
			int x(0);

			for(auto &n : m_resource)
				x = 10*x + n;
		}
	};

	/// <summary>
	/// Struct used in the GC test for cyclic reference.
	/// </summary>
	struct Nexus
	{
		int m_seqId;

		sptr<Nexus> m_next;

		Nexus(int seqId) :
			m_seqId(seqId)
		{
		}
	};

	/// <summary>
	/// Mock-up object model to be managed by the GC.
	/// </summary>
	struct Thing
	{
#		ifdef NDEBUG
		static const int maxDepth = 19;
#		else
		static const int maxDepth = 14;
#		endif

		int m_deep;

		sptr<Thing> m_left;
		sptr<Thing> m_right;

		Thing(int parentDeep) :
			m_deep(parentDeep + 1)
		{
			if (m_deep < maxDepth)
			{
				m_left.has(Thing(m_deep));
				m_right.has(Thing(m_deep));
			}
		}
	};

	/// <summary>
	/// Dummy class for stress test of the GC.
	/// </summary>
	struct Foo
	{
		int m_dummyMember1,
			m_dummyMember2;

		long long m_dummyMember3;

		sptr<Foo> m_any;
	};

	/// <summary>
	/// Tests the garbage collector for copy of safe pointers.
	/// </summary>
	TEST(Framework_MemoryGC_TestCase, CopySemantics_Test)
	{
		// Ensures proper initialization/finalization of the framework
#   ifdef _3FD_PLATFORM_WINRT
        core::FrameworkInstance _framework("IntegrationTestsApp.WinRT.UWP");
#   else
        core::FrameworkInstance _framework;
#   endif

		CALL_STACK_TRACE;

		try
		{
			auto createResourceHolder = []()
			{
				sptr<ResourceHolder> x;
				x.has(ResourceHolder());
				return x;
			};

			sptr<ResourceHolder> y = createResourceHolder();
		}
		catch (...)
		{
			HandleException();
		}
	}

	/// <summary>
	/// Tests the GC behavior when the construction of an object fails.
	/// </summary>
	TEST(Framework_MemoryGC_TestCase, CtorFailure_Test)
	{
		// Ensures proper initialization/finalization of the framework
#   ifdef _3FD_PLATFORM_WINRT
        core::FrameworkInstance _framework("IntegrationTestsApp.WinRT.UWP");
#   else
        core::FrameworkInstance _framework;
#   endif

		CALL_STACK_TRACE;

		try
		{
			sptr<ResourceHolder> x;
			x.has(ResourceHolder());

			sptr<ResourceHolder> y = x;
			y.has(ResourceHolder(true));
		}
		catch (...)
		{
			// Do nothing.
		}
	}

	/// <summary>
	/// Tests the GC for the resolution of memory management of cyclic references.
	/// </summary>
	TEST(Framework_MemoryGC_TestCase, RefCycles_Test)
	{
		// Ensures proper initialization/finalization of the framework
#   ifdef _3FD_PLATFORM_WINRT
        core::FrameworkInstance _framework("IntegrationTestsApp.WinRT.UWP");
#   else
        core::FrameworkInstance _framework;
#   endif

		/* This macro tells the framework to trace this call. If you do not use it,
		this frame will not be visible in the stack trace report. */
		CALL_STACK_TRACE;

		// Once the framework instance has been initialized, you can start to use its features:
		try
		{
			// Create a sptr object
			sptr<Nexus> begin;
			begin.has(Nexus(0)); // Assign a dinamically allocated object to it using the macro 'has'
			begin->m_next.has(Nexus(1));
			begin->m_next->m_next.has(Nexus(2));
			begin->m_next->m_next->m_next.has(Nexus(3));
			begin->m_next->m_next->m_next->m_next.has(Nexus(4));
			begin->m_next->m_next->m_next->m_next->m_next = begin; // closes the cycle
		}
		catch (...)
		{
			HandleException();
		}
	}

	/// <summary>
	/// Tests the GC for allocation of objects in a tree structure with cycles.
	/// </summary>
	TEST(Framework_MemoryGC_TestCase, LargeBinaryTree_Test)
	{
		// Ensures proper initialization/finalization of the framework
#   ifdef _3FD_PLATFORM_WINRT
        core::FrameworkInstance _framework("IntegrationTestsApp.WinRT.UWP");
#   else
        core::FrameworkInstance _framework;
#   endif

		CALL_STACK_TRACE;

		try
		{
			sptr<Thing> root;
			root.has(Thing(0));
			root->m_right->m_right->m_right.Reset();
			root->m_left->m_left->m_left = root;
			root->m_right->m_left->m_left->m_left = root->m_left->m_right->m_right;
			root.Reset();
		}
		catch (...)
		{
			HandleException();
		}
	}

	/// <summary>
	/// Tests the GC in a simulation of a real world stressful scenario.
	/// </summary>
	static void RealWorldStress_TestImplementation(size_t qtObjects)
	{
		// Ensures proper initialization/finalization of the framework
#   ifdef _3FD_PLATFORM_WINRT
        core::FrameworkInstance _framework("IntegrationTestsApp.WinRT.UWP");
#   else
        core::FrameworkInstance _framework;
#   endif

		CALL_STACK_TRACE;

		try
		{
			// Create N (> 3) layers of garbage collectable objects:

			int divisor(1);
			std::vector<std::vector<sptr<Foo>>> objectsLayers(7);

			generate(objectsLayers.begin(), objectsLayers.end(), [qtObjects, &divisor]()
			{
				std::vector<sptr<Foo>> layer(qtObjects / divisor++);

				for (auto &pointer : layer)
					pointer.has(Foo());

				return std::move(layer);
			});

			// Make random references between objects of adjacent layers:

			std::random_device randomDevice;
			std::ranlux24_base randomEngine(randomDevice());
			std::uniform_real_distribution<double> uniformDistribution(0, 1.0);

			for (int layerIdx = 0; layerIdx < objectsLayers.size() - 1; ++layerIdx)
			{
				auto &layer = objectsLayers[layerIdx];
				auto &nextLayer = objectsLayers[layerIdx + 1];
				const auto nextLayerSize = nextLayer.size();

				for (int objIdx = 0; objIdx < layer.size(); ++objIdx)
				{
					auto &object = layer[objIdx];
					auto randomIdx = static_cast<int> (uniformDistribution(randomEngine) * nextLayerSize - 0.5);

					object->m_any = nextLayer[randomIdx];
				}

				uniformDistribution.reset();
				randomEngine.seed(randomDevice());
			}

			// Make the objects of the last layer randomly reference the ones of the second layer:

			auto &secondLayer = objectsLayers[1];

			for (auto &object : *objectsLayers.rbegin())
			{
				auto randomIdx = static_cast<int> (uniformDistribution(randomEngine) * secondLayer.size() - 0.5);

				object->m_any = secondLayer[randomIdx];
			}

			// Keep only the first layer of pointers
			objectsLayers.resize(1);

			// Destroy the remaining access points one by one:

			auto &firstLayer = objectsLayers[0];

			while (firstLayer.empty() == false)
				firstLayer.pop_back();
		}
		catch (...)
		{
			HandleException();
		}
	}

	class Framework_MemoryGC_TestCase : public ::testing::TestWithParam<size_t> { };

	TEST_P(Framework_MemoryGC_TestCase, RealWorldStress_Test)
	{
		RealWorldStress_TestImplementation(GetParam());
	}

#ifdef NDEBUG
	INSTANTIATE_TEST_CASE_P(RealWorldStress_Test,
		Framework_MemoryGC_TestCase,
		::testing::Values(1e4, 2e4, 4e4));
#else
	INSTANTIATE_TEST_CASE_P(RealWorldStress_Test,
		Framework_MemoryGC_TestCase,
		::testing::Values(500, 1000, 2000, 4000));
#endif


}// end of namespace integration_tests
}// end of namespace _3fd
