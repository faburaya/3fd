//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include "concurrency.h"
#include <cassert>

namespace _3fd
{
namespace utils
{
    ////////////////////////////////////
    // shared_mutex Class
    ////////////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="shared_mutex"/> class.
    /// </summary>
    shared_mutex::shared_mutex()
        : m_curLockType(None)
    {
        InitializeSRWLock(&m_srwLockHandle);
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="shared_mutex"/> class.
    /// </summary>
    shared_mutex::~shared_mutex()
    {
        switch (m_curLockType)
        {
        case _3fd::utils::shared_mutex::Shared:
            unlock_shared();
            break;

        case _3fd::utils::shared_mutex::Exclusive:
            unlock();
            break;

        default:
            break;
        }
    }

    /// <summary>
    /// Acquires a shared lock.
    /// </summary>
    void shared_mutex::lock_shared()
    {
        AcquireSRWLockShared(&m_srwLockHandle);
        m_curLockType = Shared;
    }

    /// <summary>
    /// Releases a shared lock.
    /// </summary>
    void shared_mutex::unlock_shared()
    {
        _ASSERTE(m_curLockType == Shared); // cannot release a lock that was not previously acquired
        ReleaseSRWLockShared(&m_srwLockHandle);
        m_curLockType = None;
    }

    /// <summary>
    /// Acquires an exclusive lock
    /// </summary>
    void shared_mutex::lock()
    {
        AcquireSRWLockExclusive(&m_srwLockHandle);
        m_curLockType = Exclusive;
    }

    /// <summary>
    /// Releases the exclusive lock.
    /// </summary>
    void shared_mutex::unlock()
    {
        _ASSERTE(m_curLockType == Exclusive); // cannot release a lock that was not previously acquired
        ReleaseSRWLockExclusive(&m_srwLockHandle);
        m_curLockType = None;
    }

} // end of namespace utils
} // end of namespace _3fd
