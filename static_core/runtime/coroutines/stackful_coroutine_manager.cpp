/**
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <algorithm>
#include "runtime/coroutines/coroutine.h"
#include "runtime/coroutines/stackful_coroutine.h"
#include "runtime/include/thread_scopes.h"
#include "libpandabase/macros.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/panda_vm.h"
#include "runtime/coroutines/stackful_coroutine_manager.h"

namespace panda {

uint8_t *StackfulCoroutineManager::AllocCoroutineStack()
{
    Pool stackPool = PoolManager::GetMmapMemPool()->AllocPool<OSPagesAllocPolicy::NO_POLICY>(
        coroStackSizeBytes_, SpaceType::SPACE_TYPE_NATIVE_STACKS, AllocatorType::NATIVE_STACKS_ALLOCATOR);
    return static_cast<uint8_t *>(stackPool.GetMem());
}

void StackfulCoroutineManager::FreeCoroutineStack(uint8_t *stack)
{
    if (stack != nullptr) {
        PoolManager::GetMmapMemPool()->FreePool(stack, coroStackSizeBytes_);
    }
}

void StackfulCoroutineManager::CreateWorkers(uint32_t howMany, Runtime *runtime, PandaVM *vm)
{
    auto allocator = Runtime::GetCurrent()->GetInternalAllocator();

    auto *wMain = allocator->New<StackfulCoroutineWorker>(
        runtime, vm, this, StackfulCoroutineWorker::ScheduleLoopType::FIBER, "[main] worker 0");
    workers_.push_back(wMain);

    for (uint32_t i = 1; i < howMany; ++i) {
        auto *w = allocator->New<StackfulCoroutineWorker>(
            runtime, vm, this, StackfulCoroutineWorker::ScheduleLoopType::THREAD, "worker " + ToPandaString(i));
        workers_.push_back(w);
    }

    auto *mainCo = CreateMainCoroutine(runtime, vm);
    mainCo->GetContext<StackfulCoroutineContext>()->SetWorker(wMain);
    Coroutine::SetCurrent(mainCo);
    activeWorkersCount_ = 1;  // 1 is for MAIN

    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::CreateWorkers(): waiting for workers startup";
    while (activeWorkersCount_ < howMany) {
        // NOTE(konstanting, #I67QXC): need timed wait?..
        workersCv_.Wait(&workersLock_);
    }
}

void StackfulCoroutineManager::OnWorkerShutdown()
{
    os::memory::LockHolder lock(workersLock_);
    --activeWorkersCount_;
    workersCv_.Signal();
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::OnWorkerShutdown(): COMPLETED, workers left = "
                           << activeWorkersCount_;
}

void StackfulCoroutineManager::OnWorkerStartup()
{
    os::memory::LockHolder lock(workersLock_);
    ++activeWorkersCount_;
    workersCv_.Signal();
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::OnWorkerStartup(): COMPLETED, active workers = "
                           << activeWorkersCount_;
}

void StackfulCoroutineManager::DisableCoroutineSwitch()
{
    GetCurrentWorker()->DisableCoroutineSwitch();
}

void StackfulCoroutineManager::EnableCoroutineSwitch()
{
    GetCurrentWorker()->EnableCoroutineSwitch();
}

bool StackfulCoroutineManager::IsCoroutineSwitchDisabled()
{
    return GetCurrentWorker()->IsCoroutineSwitchDisabled();
}

void StackfulCoroutineManager::Initialize(CoroutineManagerConfig config, Runtime *runtime, PandaVM *vm)
{
    // set limits
    coroStackSizeBytes_ = Runtime::GetCurrent()->GetOptions().GetCoroutineStackSizePages() * os::mem::GetPageSize();
    if (coroStackSizeBytes_ != AlignUp(coroStackSizeBytes_, PANDA_POOL_ALIGNMENT_IN_BYTES)) {
        size_t alignmentPages = PANDA_POOL_ALIGNMENT_IN_BYTES / os::mem::GetPageSize();
        LOG(FATAL, COROUTINES) << "Coroutine stack size should be >= " << alignmentPages
                               << " pages and should be aligned to " << alignmentPages << "-page boundary!";
    }
    size_t coroStackAreaSizeBytes = Runtime::GetCurrent()->GetOptions().GetCoroutinesStackMemLimit();
    coroutineCountLimit_ = coroStackAreaSizeBytes / coroStackSizeBytes_;
    jsMode_ = config.emulateJs;

    // create and activate workers
    uint32_t targetNumberOfWorkers = (config.workersCount == CoroutineManagerConfig::WORKERS_COUNT_AUTO)
                                         ? std::thread::hardware_concurrency()
                                         : static_cast<uint32_t>(config.workersCount);
    if (config.workersCount == CoroutineManagerConfig::WORKERS_COUNT_AUTO) {
        LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager(): AUTO mode selected, will set number of coroutine "
                                  "workers to number of CPUs = "
                               << targetNumberOfWorkers;
    }
    os::memory::LockHolder lock(workersLock_);
    CreateWorkers(targetNumberOfWorkers, runtime, vm);
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager(): successfully created and activated " << workers_.size()
                           << " coroutine workers";
    programCompletionEvent_ = Runtime::GetCurrent()->GetInternalAllocator()->New<GenericEvent>();
}

void StackfulCoroutineManager::Finalize()
{
    os::memory::LockHolder lock(coroPoolLock_);

    auto allocator = Runtime::GetCurrent()->GetInternalAllocator();
    allocator->Delete(programCompletionEvent_);
    for (auto *co : coroutinePool_) {
        co->DestroyInternalResources();
        CoroutineManager::DestroyEntrypointfulCoroutine(co);
    }
    coroutinePool_.clear();
}

void StackfulCoroutineManager::AddToRegistry(Coroutine *co)
{
    os::memory::LockHolder lock(coroListLock_);
    co->GetVM()->GetGC()->OnThreadCreate(co);
    coroutines_.insert(co);
    coroutineCount_++;
}

void StackfulCoroutineManager::RemoveFromRegistry(Coroutine *co)
{
    coroutines_.erase(co);
    coroutineCount_--;
}

void StackfulCoroutineManager::RegisterCoroutine(Coroutine *co)
{
    AddToRegistry(co);
}

bool StackfulCoroutineManager::TerminateCoroutine(Coroutine *co)
{
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::TerminateCoroutine() started";
    co->NativeCodeEnd();
    co->UpdateStatus(ThreadStatus::TERMINATING);

    {
        os::memory::LockHolder lList(coroListLock_);
        RemoveFromRegistry(co);
        // DestroyInternalResources()/CleanupInternalResources() must be called in one critical section with
        // RemoveFromRegistry (under core_list_lock_). This functions transfer cards from coro's post_barrier buffer to
        // UpdateRemsetThread internally. Situation when cards still remain and UpdateRemsetThread cannot visit the
        // coro (because it is already removed) must be impossible.
        if (Runtime::GetOptions().IsUseCoroutinePool() && co->HasManagedEntrypoint()) {
            co->CleanupInternalResources();
        } else {
            co->DestroyInternalResources();
        }
        co->UpdateStatus(ThreadStatus::FINISHED);
    }
    Runtime::GetCurrent()->GetNotificationManager()->ThreadEndEvent(co);

    if (co->HasManagedEntrypoint()) {
        UnblockWaiters(co->GetCompletionEvent());
        CheckProgramCompletion();
        GetCurrentWorker()->RequestFinalization(co);
    } else if (co->HasNativeEntrypoint()) {
        GetCurrentWorker()->RequestFinalization(co);
    } else {
        // entrypointless and NOT native: e.g. MAIN
        // (do nothing, as entrypointless coroutines should should be destroyed manually)
    }

    return false;
}

size_t StackfulCoroutineManager::GetActiveWorkersCount()
{
    os::memory::LockHolder lkWorkers(workersLock_);
    return activeWorkersCount_;
}

void StackfulCoroutineManager::CheckProgramCompletion()
{
    os::memory::LockHolder lkCompletion(programCompletionLock_);
    size_t activeWorkerCoros = GetActiveWorkersCount();

    if (coroutineCount_ == 1 + activeWorkerCoros) {  // 1 here is for MAIN
        LOG(DEBUG, COROUTINES)
            << "StackfulCoroutineManager::CheckProgramCompletion(): all coroutines finished execution!";
        // programCompletionEvent_ acts as a stackful-friendly cond var
        programCompletionEvent_->SetHappened();
        UnblockWaiters(programCompletionEvent_);
    } else {
        LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::CheckProgramCompletion(): still "
                               << coroutineCount_ - 1 - activeWorkerCoros << " coroutines exist...";
    }
}

CoroutineContext *StackfulCoroutineManager::CreateCoroutineContext(bool coroHasEntrypoint)
{
    return CreateCoroutineContextImpl(coroHasEntrypoint);
}

void StackfulCoroutineManager::DeleteCoroutineContext(CoroutineContext *ctx)
{
    FreeCoroutineStack(static_cast<StackfulCoroutineContext *>(ctx)->GetStackLoAddrPtr());
    Runtime::GetCurrent()->GetInternalAllocator()->Delete(ctx);
}

size_t StackfulCoroutineManager::GetCoroutineCount()
{
    return coroutineCount_;
}

size_t StackfulCoroutineManager::GetCoroutineCountLimit()
{
    return coroutineCountLimit_;
}

Coroutine *StackfulCoroutineManager::Launch(CompletionEvent *completionEvent, Method *entrypoint,
                                            PandaVector<Value> &&arguments, CoroutineAffinity affinity)
{
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::Launch started";

    auto *result = LaunchImpl(completionEvent, entrypoint, std::move(arguments), affinity);
    if (result == nullptr) {
        ThrowOutOfMemoryError("Launch failed");
    }

    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::Launch finished";
    return result;
}

void StackfulCoroutineManager::Await(CoroutineEvent *awaitee)
{
    ASSERT(awaitee != nullptr);
    [[maybe_unused]] auto *waiter = Coroutine::GetCurrent();
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::Await started by " + waiter->GetName();
    if (!GetCurrentWorker()->WaitForEvent(awaitee)) {
        LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::Await finished (no await happened)";
        return;
    }
    // NB: at this point the awaitee is likely already deleted
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::Await finished by " + waiter->GetName();
}

void StackfulCoroutineManager::UnblockWaiters(CoroutineEvent *blocker)
{
    os::memory::LockHolder lkWorkers(workersLock_);
    ASSERT(blocker != nullptr);
#ifndef NDEBUG
    {
        os::memory::LockHolder lkBlocker(*blocker);
        ASSERT(blocker->Happened());
    }
#endif

    for (auto *w : workers_) {
        w->UnblockWaiters(blocker);
    }
}

void StackfulCoroutineManager::Schedule()
{
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::Schedule() request from "
                           << Coroutine::GetCurrent()->GetName();
    GetCurrentWorker()->RequestSchedule();
}

bool StackfulCoroutineManager::EnumerateThreadsImpl(const ThreadManager::Callback &cb, unsigned int incMask,
                                                    unsigned int xorMask) const
{
    os::memory::LockHolder lock(coroListLock_);
    for (auto *t : coroutines_) {
        if (!ApplyCallbackToThread(cb, t, incMask, xorMask)) {
            return false;
        }
    }
    return true;
}

void StackfulCoroutineManager::SuspendAllThreads()
{
    os::memory::LockHolder lock(coroListLock_);
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::SuspendAllThreads started";
    for (auto *t : coroutines_) {
        t->SuspendImpl(true);
    }
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::SuspendAllThreads finished";
}

void StackfulCoroutineManager::ResumeAllThreads()
{
    os::memory::LockHolder lock(coroListLock_);
    for (auto *t : coroutines_) {
        t->ResumeImpl(true);
    }
}

bool StackfulCoroutineManager::IsRunningThreadExist()
{
    UNREACHABLE();
    // NOTE(konstanting): correct implementation. Which coroutine do we consider running?
    return false;
}

void StackfulCoroutineManager::WaitForDeregistration()
{
    MainCoroutineCompleted();
}

void StackfulCoroutineManager::ReuseCoroutineInstance(Coroutine *co, CompletionEvent *completionEvent,
                                                      Method *entrypoint, PandaVector<Value> &&arguments,
                                                      PandaString name)
{
    auto *ctx = co->GetContext<CoroutineContext>();
    co->ReInitialize(std::move(name), ctx,
                     Coroutine::ManagedEntrypointInfo {completionEvent, entrypoint, std::move(arguments)});
}

Coroutine *StackfulCoroutineManager::TryGetCoroutineFromPool()
{
    os::memory::LockHolder lkPool(coroPoolLock_);
    if (coroutinePool_.empty()) {
        return nullptr;
    }
    Coroutine *co = coroutinePool_.back();
    coroutinePool_.pop_back();
    return co;
}

StackfulCoroutineWorker *StackfulCoroutineManager::ChooseWorkerForCoroutine(CoroutineAffinity affinity)
{
    switch (affinity) {
        case CoroutineAffinity::SAME_WORKER: {
            return GetCurrentWorker();
        }
        case CoroutineAffinity::NONE:
        default: {
            // choosing the least loaded worker
            os::memory::LockHolder lkWorkers(workersLock_);
            auto w = std::min_element(workers_.begin(), workers_.end(),
                                      [](const StackfulCoroutineWorker *a, const StackfulCoroutineWorker *b) {
                                          return a->GetLoadFactor() < b->GetLoadFactor();
                                      });
            return *w;
        }
    }
}

Coroutine *StackfulCoroutineManager::LaunchImpl(CompletionEvent *completionEvent, Method *entrypoint,
                                                PandaVector<Value> &&arguments, CoroutineAffinity affinity)
{
#ifndef NDEBUG
    GetCurrentWorker()->PrintRunnables("LaunchImpl begin");
#endif
    auto coroName = entrypoint->GetFullName();

    Coroutine *co = nullptr;
    if (Runtime::GetOptions().IsUseCoroutinePool()) {
        co = TryGetCoroutineFromPool();
    }
    if (co != nullptr) {
        ReuseCoroutineInstance(co, completionEvent, entrypoint, std::move(arguments), std::move(coroName));
    } else {
        co = CreateCoroutineInstance(completionEvent, entrypoint, std::move(arguments), std::move(coroName));
    }
    if (co == nullptr) {
        LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::LaunchImpl: failed to create a coroutine!";
        return co;
    }
    Runtime::GetCurrent()->GetNotificationManager()->ThreadStartEvent(co);

    auto *w = ChooseWorkerForCoroutine(affinity);
    w->AddRunnableCoroutine(co, IsJsMode());

#ifndef NDEBUG
    GetCurrentWorker()->PrintRunnables("LaunchImpl end");
#endif
    return co;
}

void StackfulCoroutineManager::MainCoroutineCompleted()
{
    // precondition: MAIN is already in the native mode
    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::MainCoroutineCompleted(): STARTED";

    // block till only schedule loop coroutines are present
    LOG(DEBUG, COROUTINES)
        << "StackfulCoroutineManager::MainCoroutineCompleted(): waiting for other coroutines to complete";

    {
        os::memory::LockHolder lkCompletion(programCompletionLock_);
        auto *main = Coroutine::GetCurrent();
        while (coroutineCount_ > 1 + GetActiveWorkersCount()) {  // 1 is for MAIN
            programCompletionEvent_->SetNotHappened();
            programCompletionEvent_->Lock();
            programCompletionLock_.Unlock();
            ScopedManagedCodeThread s(main);  // perf?
            GetCurrentWorker()->WaitForEvent(programCompletionEvent_);
            LOG(DEBUG, COROUTINES)
                << "StackfulCoroutineManager::MainCoroutineCompleted(): possibly spurious wakeup from wait...";
            // NOTE(konstanting, #I67QXC): test for the spurious wakeup
            programCompletionLock_.Lock();
        }
        ASSERT(coroutineCount_ == (1 + GetActiveWorkersCount()));
    }

    // NOTE(konstanting, #I67QXC): correct state transitions for MAIN
    GetCurrentContext()->MainThreadFinished();
    GetCurrentContext()->EnterAwaitLoop();

    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::MainCoroutineCompleted(): stopping workers";
    {
        os::memory::LockHolder lock(workersLock_);
        for (auto *worker : workers_) {
            worker->SetActive(false);
        }
        while (activeWorkersCount_ > 1) {  // 1 is for MAIN
            // NOTE(konstanting, #I67QXC): need timed wait?..
            workersCv_.Wait(&workersLock_);
        }
    }

    LOG(DEBUG, COROUTINES)
        << "StackfulCoroutineManager::MainCoroutineCompleted(): stopping await loop on the main worker";
    while (coroutineCount_ > 1) {
        GetCurrentWorker()->FinalizeFiberScheduleLoop();
    }

    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::MainCoroutineCompleted(): deleting workers";
    {
        os::memory::LockHolder lkWorkers(workersLock_);
        for (auto *worker : workers_) {
            Runtime::GetCurrent()->GetInternalAllocator()->Delete(worker);
        }
    }

    LOG(DEBUG, COROUTINES) << "StackfulCoroutineManager::MainCoroutineCompleted(): DONE";
}

StackfulCoroutineContext *StackfulCoroutineManager::GetCurrentContext()
{
    auto *co = Coroutine::GetCurrent();
    return co->GetContext<StackfulCoroutineContext>();
}

StackfulCoroutineWorker *StackfulCoroutineManager::GetCurrentWorker()
{
    return GetCurrentContext()->GetWorker();
}

bool StackfulCoroutineManager::IsJsMode()
{
    return jsMode_;
}

void StackfulCoroutineManager::DestroyEntrypointfulCoroutine(Coroutine *co)
{
    if (Runtime::GetOptions().IsUseCoroutinePool() && co->HasManagedEntrypoint()) {
        co->CleanUp();
        os::memory::LockHolder lock(coroPoolLock_);
        coroutinePool_.push_back(co);
    } else {
        CoroutineManager::DestroyEntrypointfulCoroutine(co);
    }
}

StackfulCoroutineContext *StackfulCoroutineManager::CreateCoroutineContextImpl(bool needStack)
{
    uint8_t *stack = nullptr;
    size_t stackSizeBytes = 0;
    if (needStack) {
        stack = AllocCoroutineStack();
        if (stack == nullptr) {
            return nullptr;
        }
        stackSizeBytes = coroStackSizeBytes_;
    }
    return Runtime::GetCurrent()->GetInternalAllocator()->New<StackfulCoroutineContext>(stack, stackSizeBytes);
}

Coroutine *StackfulCoroutineManager::CreateNativeCoroutine(Runtime *runtime, PandaVM *vm,
                                                           Coroutine::NativeEntrypointInfo::NativeEntrypointFunc entry,
                                                           void *param, PandaString name)
{
    if (GetCoroutineCount() >= GetCoroutineCountLimit()) {
        // resource limit reached
        return nullptr;
    }
    StackfulCoroutineContext *ctx = CreateCoroutineContextImpl(true);
    if (ctx == nullptr) {
        // do not proceed if we cannot create a context for the new coroutine
        return nullptr;
    }
    auto *co = GetCoroutineFactory()(runtime, vm, std::move(name), ctx, Coroutine::NativeEntrypointInfo(entry, param));
    ASSERT(co != nullptr);

    // Let's assume that even the "native" coroutine can eventually try to execute some managed code.
    // In that case pre/post barrier buffers are necessary.
    co->InitBuffers();
    return co;
}

void StackfulCoroutineManager::DestroyNativeCoroutine(Coroutine *co)
{
    DestroyEntrypointlessCoroutine(co);
}

}  // namespace panda
