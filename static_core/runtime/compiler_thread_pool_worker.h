/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef RUTNIME_COMPILER_THREAD_POOL_WORKER_H
#define RUTNIME_COMPILER_THREAD_POOL_WORKER_H

#include "runtime/compiler_worker.h"
#include "runtime/thread_pool.h"
#include "runtime/compiler_queue_interface.h"

namespace panda {

class CompilerProcessor : public ProcessorInterface<CompilerTask, Compiler *> {
public:
    explicit CompilerProcessor(Compiler *compiler);
    bool Process(CompilerTask &&task) override;

private:
    Compiler *compiler_;
};

/// @brief Compiler worker task pool based on ThreadPool
class CompilerThreadPoolWorker : public CompilerWorker {
public:
    CompilerThreadPoolWorker(mem::InternalAllocatorPtr internal_allocator, Compiler *compiler, bool &no_async_jit,
                             const RuntimeOptions &options);
    NO_COPY_SEMANTIC(CompilerThreadPoolWorker);
    NO_MOVE_SEMANTIC(CompilerThreadPoolWorker);
    ~CompilerThreadPoolWorker() override;

    void InitializeWorker() override
    {
        thread_pool_ = internal_allocator_->New<ThreadPool<CompilerTask, CompilerProcessor, Compiler *>>(
            internal_allocator_, queue_, compiler_, 1, "JIT Thread");
    }

    void FinalizeWorker() override
    {
        if (thread_pool_ != nullptr) {
            JoinWorker();
            internal_allocator_->Delete(thread_pool_);
            thread_pool_ = nullptr;
        }
    }

    void JoinWorker() override
    {
        if (thread_pool_ != nullptr) {
            thread_pool_->Shutdown(true);
        }
    }

    bool IsWorkerJoined() override
    {
        return !thread_pool_->IsActive();
    }

    void AddTask(CompilerTask &&ctx) override
    {
        thread_pool_->PutTask(std::move(ctx));
    }

    ThreadPool<CompilerTask, CompilerProcessor, Compiler *> *GetThreadPool()
    {
        return thread_pool_;
    }

private:
    CompilerQueueInterface *CreateJITTaskQueue(const std::string &queue_type, uint64_t max_length, uint64_t task_life,
                                               uint64_t death_counter, uint64_t epoch_duration);

    // This queue is used only in ThreadPool. Do not use it from this class.
    CompilerQueueInterface *queue_ {nullptr};
    ThreadPool<CompilerTask, CompilerProcessor, Compiler *> *thread_pool_ {nullptr};
};

}  // namespace panda

#endif  // RUTNIME_COMPILER_THREAD_POOL_WORKER_H
