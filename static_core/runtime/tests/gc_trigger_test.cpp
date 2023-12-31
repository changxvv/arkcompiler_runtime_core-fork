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

#include <gtest/gtest.h>
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/thread_scopes.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/gc/gc_trigger.h"
#include "runtime/handle_scope-inl.h"
#include "test_utils.h"

namespace panda::test {
class GCTriggerTest : public testing::Test {
public:
    GCTriggerTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetGcTriggerType("adaptive-heap-trigger");

        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~GCTriggerTest() override
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

    NO_COPY_SEMANTIC(GCTriggerTest);
    NO_MOVE_SEMANTIC(GCTriggerTest);

    [[nodiscard]] mem::GCAdaptiveTriggerHeap *CreateGCTriggerHeap() const
    {
        return new mem::GCAdaptiveTriggerHeap(nullptr, nullptr, MIN_HEAP_SIZE,
                                              mem::GCTriggerHeap::DEFAULT_PERCENTAGE_THRESHOLD,
                                              DEFAULT_INCREASE_MULTIPLIER, MIN_EXTRA_HEAP_SIZE, MAX_EXTRA_HEAP_SIZE);
    }

    static size_t GetTargetFootprint(const mem::GCAdaptiveTriggerHeap *trigger)
    {
        // Atomic with relaxed order reason: simple getter for test
        return trigger->target_footprint_.load(std::memory_order_relaxed);
    }

protected:
    static constexpr size_t MIN_HEAP_SIZE = 8_MB;
    static constexpr size_t MIN_EXTRA_HEAP_SIZE = 1_MB;
    static constexpr size_t MAX_EXTRA_HEAP_SIZE = 8_MB;
    static constexpr uint32_t DEFAULT_INCREASE_MULTIPLIER = 3U;

private:
    MTManagedThread *thread_ = nullptr;
};

TEST_F(GCTriggerTest, ThresholdTest)
{
    static constexpr size_t BEFORE_HEAP_SIZE = 50_MB;
    static constexpr size_t CURRENT_HEAP_SIZE = MIN_HEAP_SIZE;
    static constexpr size_t FIRST_THRESHOLD = 2U * MIN_HEAP_SIZE;
    static constexpr size_t HEAP_SIZE_AFTER_BASE_TRIGGER = (BEFORE_HEAP_SIZE + CURRENT_HEAP_SIZE) / 2U;
    auto *trigger = CreateGCTriggerHeap();
    GCTask task(GCTaskCause::HEAP_USAGE_THRESHOLD_CAUSE);

    trigger->ComputeNewTargetFootprint(task, BEFORE_HEAP_SIZE, CURRENT_HEAP_SIZE);

    ASSERT_EQ(GetTargetFootprint(trigger), FIRST_THRESHOLD);

    trigger->ComputeNewTargetFootprint(task, BEFORE_HEAP_SIZE, CURRENT_HEAP_SIZE);
    ASSERT_EQ(GetTargetFootprint(trigger), HEAP_SIZE_AFTER_BASE_TRIGGER);
    trigger->ComputeNewTargetFootprint(task, BEFORE_HEAP_SIZE, CURRENT_HEAP_SIZE);
    ASSERT_EQ(GetTargetFootprint(trigger), HEAP_SIZE_AFTER_BASE_TRIGGER);
    trigger->ComputeNewTargetFootprint(task, BEFORE_HEAP_SIZE, CURRENT_HEAP_SIZE);
    ASSERT_EQ(GetTargetFootprint(trigger), HEAP_SIZE_AFTER_BASE_TRIGGER);
    trigger->ComputeNewTargetFootprint(task, BEFORE_HEAP_SIZE, CURRENT_HEAP_SIZE);

    // Check that we could to avoid locale triggering
    ASSERT_EQ(GetTargetFootprint(trigger), FIRST_THRESHOLD);

    delete trigger;
}

class GCChecker : public mem::GCListener {
public:
    void GCFinished(const GCTask &task, [[maybe_unused]] size_t heap_size_before_gc,
                    [[maybe_unused]] size_t heap_size) override
    {
        reason_ = task.reason;
        counter_++;
    }

    GCTaskCause GetCause() const
    {
        return reason_;
    }

    size_t GetCounter() const
    {
        return counter_;
    }

private:
    GCTaskCause reason_ = GCTaskCause::INVALID_CAUSE;
    size_t counter_ {0};
};

TEST(SchedGCOnNthAllocTriggerTest, TestTrigger)
{
    RuntimeOptions options;
    options.SetShouldLoadBootPandaFiles(false);
    options.SetShouldInitializeIntrinsics(false);
    options.SetGcTriggerType("debug-never");
    options.SetGcUseNthAllocTrigger(true);
    Runtime::Create(options);
    ManagedThread *thread = panda::ManagedThread::GetCurrent();
    thread->ManagedCodeBegin();
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    PandaVM *vm = Runtime::GetCurrent()->GetPandaVM();
    auto *trigger = vm->GetGCTrigger();
    ASSERT_EQ(mem::GCTriggerType::ON_NTH_ALLOC, trigger->GetType());
    auto *sched_trigger = reinterpret_cast<mem::SchedGCOnNthAllocTrigger *>(trigger);
    GCChecker checker;
    vm->GetGC()->AddListener(&checker);

    sched_trigger->ScheduleGc(GCTaskCause::YOUNG_GC_CAUSE, 2);
    coretypes::String::CreateEmptyString(ctx, vm);
    EXPECT_FALSE(sched_trigger->IsTriggered());
    EXPECT_EQ(GCTaskCause::INVALID_CAUSE, checker.GetCause());
    coretypes::String::CreateEmptyString(ctx, vm);
    EXPECT_EQ(GCTaskCause::YOUNG_GC_CAUSE, checker.GetCause());
    EXPECT_TRUE(sched_trigger->IsTriggered());

    thread->ManagedCodeEnd();
    Runtime::Destroy();
}

TEST(PauseTimeGoalTriggerTest, TestTrigger)
{
    RuntimeOptions options;
    options.SetShouldLoadBootPandaFiles(false);
    options.SetShouldInitializeIntrinsics(false);
    options.SetGcTriggerType("pause-time-goal-trigger");
    options.SetRunGcInPlace(true);
    constexpr size_t YOUNG_SIZE = 512 * 1024;
    options.SetYoungSpaceSize(YOUNG_SIZE);
    options.SetInitYoungSpaceSize(YOUNG_SIZE);
    Runtime::Create(options);
    auto *thread = panda::ManagedThread::GetCurrent();
    {
        ScopedManagedCodeThread s(thread);
        HandleScope<ObjectHeader *> scope(thread);

        auto *runtime = Runtime::GetCurrent();
        auto ctx = runtime->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *vm = runtime->GetPandaVM();
        auto *trigger = vm->GetGCTrigger();
        ASSERT_EQ(mem::GCTriggerType::PAUSE_TIME_GOAL_TRIGGER, trigger->GetType());
        GCChecker checker;
        vm->GetGC()->AddListener(&checker);

        auto *pause_time_goal_trigger = static_cast<panda::mem::PauseTimeGoalTrigger *>(trigger);
        constexpr size_t INIT_TARGET_FOOTPRINT = 1258291;
        ASSERT_EQ(INIT_TARGET_FOOTPRINT, pause_time_goal_trigger->GetTargetFootprint());

        constexpr size_t ARRAY_LENGTH = 5 * 32 * 1024;  // big enough to provoke several collections
        VMHandle<coretypes::String> dummy(thread, coretypes::String::CreateEmptyString(ctx, vm));
        VMHandle<coretypes::Array> array(
            thread, panda::mem::ObjectAllocator::AllocArray(ARRAY_LENGTH, ClassRoot::ARRAY_STRING, false));

        size_t expected_counter = 1;
        size_t start_idx = 0;
        for (size_t i = 0; i < ARRAY_LENGTH; i++) {
            auto *str = coretypes::String::CreateEmptyString(ctx, vm);
            array->Set(i, str);

            auto collection = expected_counter == checker.GetCounter();
            if (collection) {
                // objects become garbage
                for (size_t j = start_idx; j < i; j++) {
                    array->Set(j, dummy.GetPtr());
                    start_idx = i;
                }
                if (expected_counter < 2) {
                    ASSERT_EQ(GCTaskCause::YOUNG_GC_CAUSE, checker.GetCause());
                } else if (expected_counter == 2) {
                    ASSERT_EQ(GCTaskCause::HEAP_USAGE_THRESHOLD_CAUSE, checker.GetCause());
                } else if (expected_counter == 3) {
                    ASSERT_EQ(GCTaskCause::YOUNG_GC_CAUSE, checker.GetCause());
                } else if (expected_counter > 3) {
                    break;
                }

                expected_counter++;
            }
        }

        ASSERT_GT(expected_counter, 3);

        // previous mixed collection should update target footprint
        ASSERT_GT(pause_time_goal_trigger->GetTargetFootprint(), INIT_TARGET_FOOTPRINT);
    }
    Runtime::Destroy();
}
}  // namespace panda::test
