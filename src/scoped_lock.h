#pragma once

#include <core_mutex.h>

class ScopedLock{
private:
    CoreMutexHandle* mutex;
public:
    ScopedLock(CoreMutexHandle* _mutex) : mutex(_mutex)
    {
        while (!CoreMutex::try_lock(mutex)) CoreMutex::wait_until_freed(mutex);
    }

    ~ScopedLock()
    {
        CoreMutex::release(mutex);
    }
};
