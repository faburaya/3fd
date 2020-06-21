//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#ifndef UTILS_CONCURRENCY_H // header guard
#define UTILS_CONCURRENCY_H

#include <3fd/core/exceptions.h>

#include <cinttypes>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#ifdef _3FD_PLATFORM_WINRT
#   include <3fd/utils/lockfreequeue.h>
#endif

namespace _3fd
{
namespace utils
{
    /// <summary>
    /// Implements an event for thread synchronization making 
    /// use of a lightweight mutex and a condition variable.
    /// </summary>
    class Event
    {
    private:

        std::mutex m_mutex;
        std::condition_variable m_condition;
        bool m_flag;

    public:

        Event();

        void Signalize();

        void Wait(const std::function<bool()> &predicate);

        bool WaitFor(unsigned long millisecs);
    };

    /// <summary>
    /// Provides helpers for asynchronous callbacks.
    /// </summary>
    class Asynchronous
    {
    public:

        static void InvokeAndLeave(const std::function<void()> &callback);
    };

#ifdef _WIN32

    /// <summary>
    /// Alternative implementation for std::shared_mutex from C++14 Std library.
    /// </summary>
    class shared_mutex
    {
    private:

        enum LockType : short
        {
            None, Shared, Exclusive
        };

        SRWLOCK m_srwLockHandle;
        LockType m_curLockType;

    public:

        shared_mutex();
        ~shared_mutex();

        shared_mutex(const shared_mutex &) = delete;

        void lock_shared();
        void unlock_shared();

        void lock();
        void unlock();
    };

#   undef GetObject
#endif

    /// <summary>
    /// Caches resources that are supposed to be used simultaneously by many.
    /// </summary>
    template <typename KeyType, typename CachedType>
    class CacheForSharedResources
    {
    private:

        std::shared_mutex m_accessToMapMutex;

        typedef std::unordered_map<KeyType, std::weak_ptr<CachedType>> Dictionary;

        Dictionary m_objects;

        typedef std::function<CachedType *(void)> Factory;

        Factory createObject;

        /// <summary>
        /// Decides whether cache should be cleaned up.
        /// </summary>
        bool ShouldCleanUpCache()
        {
            // get a random object:
            auto iter = m_objects.begin();
            if (iter == m_objects.end())
                return false;

            // rolled the dice... if dead, clean it up:
            return (iter->second.use_count() == 0);
        }

        /// <summary>
        /// MAY ONLY BE CALLED WITHIN EXCLUSIVE LOCK!!!
        /// Clean up cache from already deallocated objects.
        /// </summary>
        void CleanUpCache()
        {
            Dictionary temp;

            for (auto &pair : m_objects)
            {
                if (pair.second.use_count() > 0)
                    temp.insert(std::make_pair(pair.first, pair.second));
            }

            temp.swap(m_objects);
        }

    public:

        /// <summary>
        /// Constructor for <see cref="CacheForSharedResources"/>.
        /// </summary>
        /// <param name="objectFactory">Callback for creation of cached objects.</param>
        CacheForSharedResources(const Factory &objectFactory)
            : createObject(objectFactory)
        {
        }

        /// <summary>
        /// Constructor for <see cref="CacheForSharedResources"/>.
        /// </summary>
        CacheForSharedResources()
            : createObject([](){ return dbg_new CachedType(); })
        {
        }

        // not copiable
        CacheForSharedResources(const CacheForSharedResources &) = delete;

        /// <summary>
        /// Constructor for <see cref="CacheForSharedResources"/>.
        /// </summary>
        /// <param name="key">
        /// Key for identification of the object to retrieve (or create, when not available.)
        /// </param>
        std::shared_ptr<CachedType> GetObject(const KeyType &key)
        {
            bool foundObjectDeadInCache(false);

            {// object found in cache:
                std::shared_lock<std::shared_mutex> readLock(m_accessToMapMutex);
                auto iter = m_objects.find(key);
                if (iter != m_objects.end())
                {
                    foundObjectDeadInCache = (iter->second.use_count() == 0);
                    if (!foundObjectDeadInCache)
                        return iter->second.lock();
                }
            }
            
            {// object not available OR alive in cache
                std::unique_lock<std::shared_mutex> writeLock(m_accessToMapMutex);

                if (foundObjectDeadInCache && ShouldCleanUpCache())
                    CleanUpCache();

                std::shared_ptr<CachedType> newObject(createObject());
                m_objects[key] = newObject;
                return newObject;
            }
        }

    }; // end of class CacheDictionary

}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
