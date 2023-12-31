/**
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "global_object_lock.h"
#include "runtime/include/thread_scopes.h"

namespace panda {
// Use some global lock as fast solution.
static os::memory::RecursiveMutex MTX;    // NOLINT(fuchsia-statically-constructed-objects)
static os::memory::ConditionVariable CV;  // NOLINT(fuchsia-statically-constructed-objects)

GlobalObjectLock::GlobalObjectLock([[maybe_unused]] const ObjectHeader *obj)
{
    ScopedChangeThreadStatus s(ManagedThread::GetCurrent(), ThreadStatus::IS_BLOCKED);
    MTX.Lock();
}

bool GlobalObjectLock::Wait([[maybe_unused]] bool ignore_interruption) const
{
    ScopedChangeThreadStatus s(ManagedThread::GetCurrent(), ThreadStatus::IS_WAITING);
    CV.Wait(&MTX);
    return true;
}

bool GlobalObjectLock::TimedWait(uint64_t timeout) const
{
    ScopedChangeThreadStatus s(ManagedThread::GetCurrent(), ThreadStatus::IS_TIMED_WAITING);
    CV.TimedWait(&MTX, timeout);
    return true;
}

void GlobalObjectLock::Notify()
{
    CV.Signal();
}

void GlobalObjectLock::NotifyAll()
{
    CV.SignalAll();
}

GlobalObjectLock::~GlobalObjectLock()
{
    MTX.Unlock();
}
}  // namespace panda
