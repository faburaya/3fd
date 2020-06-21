//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include <3fd/utils/memory.h>

#include <vector>
#include <deque>

namespace _3fd
{
namespace unit_tests
{
    /// <summary>
    /// Something to be stuffed into an object pool.
    /// </summary>
    class MyClass
    {
    private:

        std::deque<int> m_integers;

        static utils::DynamicMemPool *dynMemPool;

    public:

        static void SetMemoryPool(utils::DynamicMemPool &ob)
        {
            dynMemPool = &ob;
        }

        void *operator new(size_t)
        {
            return dynMemPool->GetFreeBlock();
        }

        void operator delete(void *ptr)
        {
            dynMemPool->ReturnBlock(ptr);
        }

        MyClass()
            : m_integers(32) {}

        void Fill()
        {
            for (int index = 0; index < m_integers.size(); ++index)
                m_integers[index] = index * index;
        }

        void ShowContent()
        {
#ifdef _3FD_CONSOLE_AVAILABLE
            std::cout << "Content of the object " << this << ": ";

            for (int index = 0; index < m_integers.size(); ++index)
                std::cout << m_integers[index] << " ";

            std::cout << '\n';
#endif
        }
    };

    utils::DynamicMemPool * MyClass::dynMemPool(nullptr);

    /// <summary>
    /// Generic tests for <see cref="utils::DynamicMemPool"/> class.
    /// </summary>
    TEST(Framework_Utils_TestCase, DynamicMemPool_BasicTest)
    {
        const size_t poolSize = 2048;

        utils::DynamicMemPool myPool(poolSize, sizeof(MyClass), 1.0F);

        MyClass::SetMemoryPool(myPool);

        std::vector<MyClass *> myObjects(poolSize * 4);

        // Creating the objects for the first time:
        for (uint32_t index = 0; index < myObjects.size(); ++index)
        {
            myObjects[index] = new MyClass;
            myObjects[index]->Fill();
        }

        //myObjects[0]->ShowContent();

        // Return all objects to the pool:
        for (uint32_t index = 0; index < myObjects.size(); ++index)
            delete myObjects[index];

        // After the objects have been returned to the pool, they are gotten again:
        for (uint32_t index = 0; index < myObjects.size(); ++index)
            myObjects[index] = new MyClass;

        //myObjects[0]->ShowContent();

        // And this time, return only half the total of objects:
        auto half = myObjects.size() / 2;
        for (uint32_t index = 0; index < half; ++index)
            delete myObjects[index];

        // Then shrink to cut out an eventual excess:
        myPool.Shrink();

        // Reclaim the returned half:
        for (uint32_t index = 0; index < half; ++index)
            myObjects[index] = new MyClass;

        // Return all objects to the pool:
        for (uint32_t index = 0; index < myObjects.size(); ++index)
            delete myObjects[index];

        myPool.Shrink();
    }

}// end of namespace unit_tests
}// end of namespace _3fd
