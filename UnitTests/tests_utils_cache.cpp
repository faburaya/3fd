//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include <3fd/utils/utils_concurrency.h>

#include <future>
#include <string>
#include <thread>
#include <vector>

namespace _3fd
{
namespace unit_tests
{
    using utils::CacheForSharedResources;

    /// <summary>
    /// Test fixture for <see cref="utils::CacheForSharedResources"/>.
    /// </summary>
    class CacheForSharedResourcesTest : public ::testing::Test
    {
    public:

        const std::string_view expectedContent = "foobar";

        typedef CacheForSharedResources<int, std::string> Cache;

        std::unique_ptr<Cache> cache;

        virtual void SetUp()
        {
            auto factory = [this]()
            {
                return dbg_new std::string(expectedContent);
            };

            cache.reset(dbg_new Cache(factory));
        }

        typedef std::vector<std::shared_ptr<std::string>> Objects;

        Objects GetObjectsFromCacheSingleThread()
        {
            const int numObjects(64);
            Objects objects;
            objects.reserve(numObjects);
            for (int key = 0; key < numObjects; ++key)
            {
                objects.push_back(cache->GetObject(key));
            }

            return objects;
        }

        Objects GetObjectsFromCacheConcurrently()
        {
            const uint32_t numThreads = std::thread::hardware_concurrency();

            std::vector<std::future<Objects>> futures;
            futures.reserve(numThreads);
            for (uint32_t count = 0; count < numThreads; ++count)
            {
                futures.push_back(
                    std::async(std::launch::async,
                               &CacheForSharedResourcesTest::GetObjectsFromCacheSingleThread,
                               this)
                );
            }

            Objects allObjects;
            for (auto &future : futures)
            {
                Objects objects = future.get();
                std::copy(objects.cbegin(),
                          objects.cend(),
                          std::back_inserter(allObjects));
            }

            return allObjects;
        }
    };

    TEST_F(CacheForSharedResourcesTest, SingleThread_Fill_ClearAtOnce_Refill)
    {
        auto liveObjects = GetObjectsFromCacheSingleThread();

        for (auto &object : liveObjects)
        {
            EXPECT_EQ(expectedContent, *object);
        }

        liveObjects.clear();
        liveObjects = GetObjectsFromCacheSingleThread();

        for (auto &object : liveObjects)
        {
            EXPECT_EQ(expectedContent, *object);
        }
    }

    TEST_F(CacheForSharedResourcesTest, SingleThread_Fill_ClearOneByOne_Refill)
    {
        auto liveObjects = GetObjectsFromCacheSingleThread();

        for (auto &object : liveObjects)
        {
            EXPECT_EQ(expectedContent, *object);
            object.reset();
        }

        liveObjects.clear();
        liveObjects = GetObjectsFromCacheSingleThread();

        for (auto &object : liveObjects)
        {
            EXPECT_EQ(expectedContent, *object);
        }
    }

    TEST_F(CacheForSharedResourcesTest, Concurrent_Fill_ClearAtOnce_Refill)
    {
        auto liveObjects = GetObjectsFromCacheConcurrently();

        for (auto &object : liveObjects)
        {
            EXPECT_EQ(expectedContent, *object);
        }

        liveObjects.clear();
        liveObjects = GetObjectsFromCacheConcurrently();

        for (auto &object : liveObjects)
        {
            EXPECT_EQ(expectedContent, *object);
        }
    }

    TEST_F(CacheForSharedResourcesTest, Concurrent_Fill_ClearOneByOne_Refill)
    {
        auto liveObjects = GetObjectsFromCacheConcurrently();

        for (auto &object : liveObjects)
        {
            EXPECT_EQ(expectedContent, *object);
            object.reset();
        }

        liveObjects.clear();
        liveObjects = GetObjectsFromCacheConcurrently();

        for (auto &object : liveObjects)
        {
            EXPECT_EQ(expectedContent, *object);
        }
    }

} // end of namespaces unit_tests
} // end of namespace _3fd