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

#include <sys/mman.h>

#include "libpandabase/os/mem.h"
#include "libpandabase/utils/logger.h"
#include "runtime/mem/runslots_allocator-inl.h"
#include "runtime/mem/runslots_allocator_stl_adapter.h"
#include "runtime/tests/allocator_test_base.h"

namespace panda::mem {

using NonObjectAllocator = RunSlotsAllocator<EmptyAllocConfigWithCrossingMap>;
using RunSlotsType = RunSlots<>;

class RunSlotsAllocatorTest : public AllocatorTest<NonObjectAllocator> {
public:
    NO_COPY_SEMANTIC(RunSlotsAllocatorTest);
    NO_MOVE_SEMANTIC(RunSlotsAllocatorTest);

    // NOLINTNEXTLINE(modernize-use-equals-default)
    RunSlotsAllocatorTest()
    {
        // Logger::InitializeStdLogging(Logger::Level::DEBUG, Logger::Component::ALL);
    }

    ~RunSlotsAllocatorTest() override
    {
        for (auto i : allocated_mem_mmap_) {
            panda::os::mem::UnmapRaw(std::get<0>(i), std::get<1>(i));
        }
        // Logger::Destroy();
    }

protected:
    static constexpr size_t DEFAULT_POOL_SIZE_FOR_ALLOC = NonObjectAllocator::GetMinPoolSize();
    static constexpr size_t DEFAULT_POOL_ALIGNMENT_FOR_ALLOC = RUNSLOTS_ALIGNMENT_IN_BYTES;
    static constexpr Alignment RUNSLOTS_LOG_MAX_ALIGN = LOG_ALIGN_8;

    void AddMemoryPoolToAllocator(NonObjectAllocator &alloc) override
    {
        os::memory::LockHolder lock(pool_lock_);
        void *mem = panda::os::mem::MapRWAnonymousRaw(DEFAULT_POOL_SIZE_FOR_ALLOC);
        std::pair<void *, size_t> new_pair {mem, DEFAULT_POOL_SIZE_FOR_ALLOC};
        allocated_mem_mmap_.push_back(new_pair);
        if (!alloc.AddMemoryPool(mem, DEFAULT_POOL_SIZE_FOR_ALLOC)) {
            ASSERT_TRUE(0 && "Can't add mem pool to allocator");
        }
    }

    void AddMemoryPoolToAllocatorProtected(NonObjectAllocator &alloc) override
    {
        os::memory::LockHolder lock(pool_lock_);
        void *mem = panda::os::mem::MapRWAnonymousRaw(DEFAULT_POOL_SIZE_FOR_ALLOC + PAGE_SIZE);
        mprotect(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(mem) + DEFAULT_POOL_SIZE_FOR_ALLOC), PAGE_SIZE,
                 PROT_NONE);
        std::pair<void *, size_t> new_pair {mem, DEFAULT_POOL_SIZE_FOR_ALLOC + PAGE_SIZE};
        allocated_mem_mmap_.push_back(new_pair);
        if (!alloc.AddMemoryPool(mem, DEFAULT_POOL_SIZE_FOR_ALLOC)) {
            ASSERT_TRUE(0 && "Can't add mem pool to allocator");
        }
    }

    void ReleasePages(NonObjectAllocator &alloc)
    {
        alloc.ReleaseEmptyRunSlotsPagesUnsafe();
    }

    bool AllocatedByThisAllocator(NonObjectAllocator &allocator, void *mem) override
    {
        return allocator.AllocatedByRunSlotsAllocator(mem);
    }

    void TestRunSlots(size_t slots_size)
    {
        LOG(DEBUG, ALLOC) << "Test RunSlots with size " << slots_size;
        void *mem = aligned_alloc(RUNSLOTS_ALIGNMENT_IN_BYTES, RUNSLOTS_SIZE);
        auto runslots = reinterpret_cast<RunSlotsType *>(mem);
        runslots->Initialize(slots_size, ToUintPtr(mem), true);
        int i = 0;
        while (runslots->PopFreeSlot() != nullptr) {
            i++;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
        free(mem);
        LOG(DEBUG, ALLOC) << "Iteration = " << i;
    }

private:
    std::vector<std::pair<void *, size_t>> allocated_mem_mmap_;
    // Mutex, which allows only one thread to add pool to the pool vector
    os::memory::Mutex pool_lock_;
};

TEST_F(RunSlotsAllocatorTest, SimpleRunSlotsTest)
{
    for (size_t i = RunSlotsType::ConvertToPowerOfTwoUnsafe(RunSlotsType::MinSlotSize());
         i <= RunSlotsType::ConvertToPowerOfTwoUnsafe(RunSlotsType::MaxSlotSize()); i++) {
        TestRunSlots(1U << i);
    }
}

TEST_F(RunSlotsAllocatorTest, SimpleAllocateDifferentObjSizeTest)
{
    LOG(DEBUG, ALLOC) << "SimpleAllocateDifferentObjSizeTest";
    mem::MemStatsType mem_stats;
    NonObjectAllocator allocator(&mem_stats);
    AddMemoryPoolToAllocator(allocator);
    // NOLINTNEXTLINE(readability-magic-numbers)
    for (size_t i = 23; i < 300; i++) {
        void *mem = allocator.Alloc(i);
        (void)mem;
        LOG(DEBUG, ALLOC) << "Allocate obj with size " << i << " at " << std::hex << mem;
    }
}

TEST_F(RunSlotsAllocatorTest, TestReleaseRunSlotsPagesTest)
{
    static constexpr size_t ALLOC_SIZE = RunSlotsType::ConvertToPowerOfTwoUnsafe(RunSlotsType::MinSlotSize());
    LOG(DEBUG, ALLOC) << "TestRunSlotsReusageTestTest";
    mem::MemStatsType mem_stats;
    NonObjectAllocator allocator(&mem_stats);
    AddMemoryPoolToAllocator(allocator);
    std::vector<void *> elements;
    // Fill the whole pool
    while (true) {
        void *mem = allocator.Alloc(ALLOC_SIZE);
        if (mem == nullptr) {
            break;
        }
        elements.push_back(mem);
        LOG(DEBUG, ALLOC) << "Allocate obj with size " << ALLOC_SIZE << " at " << std::hex << mem;
    }
    // Free everything except the last element
    ASSERT(elements.size() > 1);
    size_t element_to_free_count = elements.size() - 1;
    for (size_t i = 0; i < element_to_free_count; i++) {
        allocator.Free(elements.back());
        elements.pop_back();
    }

    // ReleaseRunSlotsPages
    ReleasePages(allocator);

    // Try to allocate everything again
    for (size_t i = 0; i < element_to_free_count; i++) {
        void *mem = allocator.Alloc(ALLOC_SIZE);
        ASSERT_TRUE(mem != nullptr);
        elements.push_back(mem);
        LOG(DEBUG, ALLOC) << "Allocate obj with size " << ALLOC_SIZE << " at " << std::hex << mem;
    }

    // Free everything
    for (auto i : elements) {
        allocator.Free(i);
    }
}

TEST_F(RunSlotsAllocatorTest, AllocateAllPossibleSizesFreeTest)
{
    for (size_t i = 1; i <= RunSlotsType::MaxSlotSize(); i++) {
        AllocateAndFree(i, RUNSLOTS_SIZE / i);
    }
}

TEST_F(RunSlotsAllocatorTest, AllocateWriteFreeTest)
{
    // NOLINTNEXTLINE(readability-magic-numbers)
    AllocateAndFree(sizeof(uint64_t), 512);
}

TEST_F(RunSlotsAllocatorTest, AllocateRandomFreeTest)
{
    static constexpr size_t ALLOC_SIZE = sizeof(uint64_t);
    static constexpr size_t ELEMENTS_COUNT = 512;
    static constexpr size_t POOLS_COUNT = 1;
    AllocateFreeDifferentSizesTest<ALLOC_SIZE / 2, 2 * ALLOC_SIZE>(ELEMENTS_COUNT, POOLS_COUNT);
}

TEST_F(RunSlotsAllocatorTest, CheckReuseOfRunSlotsTest)
{
    AllocateReuseTest(RUNSLOTS_ALIGNMENT_MASK);
}

TEST_F(RunSlotsAllocatorTest, AllocateTooBigObjTest)
{
    AllocateTooBigObjectTest<RunSlotsType::MaxSlotSize()>();
}

TEST_F(RunSlotsAllocatorTest, AlignmentAllocTest)
{
    AlignedAllocFreeTest<1, RunSlotsType::MaxSlotSize(), LOG_ALIGN_MIN, RUNSLOTS_LOG_MAX_ALIGN>();
}

TEST_F(RunSlotsAllocatorTest, AllocateTooMuchTest)
{
    static constexpr size_t ALLOC_SIZE = sizeof(uint64_t);
    AllocateTooMuchTest(ALLOC_SIZE, DEFAULT_POOL_SIZE_FOR_ALLOC / ALLOC_SIZE);
}

TEST_F(RunSlotsAllocatorTest, AllocateVectorTest)
{
    AllocateVectorTest();
}

TEST_F(RunSlotsAllocatorTest, AllocateReuse2)
{
    // It's regression test
    auto *mem_stats = new mem::MemStatsType();
    NonObjectAllocator allocator(mem_stats);
    static constexpr size_t SIZE1 = 60;
    static constexpr size_t SIZE2 = 204;
    constexpr char CHAR1 = 'a';
    constexpr char CHAR2 = 'b';
    constexpr char CHAR3 = 'c';
    constexpr char CHAR4 = 'd';
    constexpr char CHAR5 = 'e';
    constexpr char CHAR6 = 'f';
    AddMemoryPoolToAllocatorProtected(allocator);
    char *str_a;
    char *str_b;
    char *str_c;
    char *str_d;
    char *str_e;
    char *str_f;
    auto fill_str = [](char *str, char c, size_t size) {
        for (size_t i = 0; i < size - 1; i++) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            str[i] = c;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        str[size - 1] = 0;
    };
    auto check_str = [](const char *str, char c, size_t size) {
        for (size_t i = 0; i < size - 1; i++) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            if (str[i] != c) {
                return false;
            }
        }
        return true;
    };
    str_a = reinterpret_cast<char *>(allocator.Alloc(SIZE1));
    str_b = reinterpret_cast<char *>(allocator.Alloc(SIZE1));
    str_c = reinterpret_cast<char *>(allocator.Alloc(SIZE1));
    fill_str(str_a, CHAR1, SIZE1);
    fill_str(str_b, CHAR2, SIZE1);
    fill_str(str_c, CHAR3, SIZE1);
    ASSERT_TRUE(check_str(str_a, CHAR1, SIZE1));
    ASSERT_TRUE(check_str(str_b, CHAR2, SIZE1));
    ASSERT_TRUE(check_str(str_c, CHAR3, SIZE1));
    allocator.Free(static_cast<void *>(str_a));
    allocator.Free(static_cast<void *>(str_b));
    allocator.Free(static_cast<void *>(str_c));
    str_d = reinterpret_cast<char *>(allocator.Alloc(SIZE2));
    str_e = reinterpret_cast<char *>(allocator.Alloc(SIZE2));
    str_f = reinterpret_cast<char *>(allocator.Alloc(SIZE2));
    fill_str(str_d, CHAR4, SIZE2);
    fill_str(str_e, CHAR5, SIZE2);
    fill_str(str_f, CHAR6, SIZE2);
    ASSERT_TRUE(check_str(str_d, CHAR4, SIZE2));
    ASSERT_TRUE(check_str(str_e, CHAR5, SIZE2));
    ASSERT_TRUE(check_str(str_f, CHAR6, SIZE2));
    delete mem_stats;
}

TEST_F(RunSlotsAllocatorTest, ObjectIteratorTest)
{
    ObjectIteratorTest<1, RunSlotsType::MaxSlotSize(), LOG_ALIGN_MIN, RUNSLOTS_LOG_MAX_ALIGN>();
}

TEST_F(RunSlotsAllocatorTest, ObjectCollectionTest)
{
    ObjectCollectionTest<1, RunSlotsType::MaxSlotSize(), LOG_ALIGN_MIN, RUNSLOTS_LOG_MAX_ALIGN>();
}

TEST_F(RunSlotsAllocatorTest, ObjectIteratorInRangeTest)
{
    ObjectIteratorInRangeTest<1, RunSlotsType::MaxSlotSize(), LOG_ALIGN_MIN, RUNSLOTS_LOG_MAX_ALIGN>(
        CrossingMapSingleton::GetCrossingMapGranularity());
}

TEST_F(RunSlotsAllocatorTest, AsanTest)
{
    AsanTest();
}

TEST_F(RunSlotsAllocatorTest, VisitAndRemoveFreePoolsTest)
{
    static constexpr size_t POOLS_COUNT = 5;
    VisitAndRemoveFreePools<POOLS_COUNT>(RunSlotsType::MaxSlotSize());
}

TEST_F(RunSlotsAllocatorTest, AllocatedByRunSlotsAllocatorTest)
{
    AllocatedByThisAllocatorTest();
}

TEST_F(RunSlotsAllocatorTest, RunSlotsReusingTest)
{
    static constexpr size_t SMALL_OBJ_SIZE = sizeof(uint32_t);
    static constexpr size_t BIG_OBJ_SIZE = 128;
    auto *mem_stats = new mem::MemStatsType();
    NonObjectAllocator allocator(mem_stats);
    AddMemoryPoolToAllocatorProtected(allocator);
    // Alloc one big object. this must cause runslots init with it size
    void *mem = allocator.Alloc(BIG_OBJ_SIZE);
    // Free this object
    allocator.Free(mem);

    // Alloc small object. We must reuse already allocated and freed RunSlots
    void *small_obj_mem = allocator.Alloc(SMALL_OBJ_SIZE);
    size_t small_obj_index = SetBytesFromByteArray(small_obj_mem, SMALL_OBJ_SIZE);

    // Alloc big obj again.
    void *big_obj_mem = allocator.Alloc(BIG_OBJ_SIZE);
    size_t big_obj_index = SetBytesFromByteArray(big_obj_mem, BIG_OBJ_SIZE);

    // Alloc one more small object.
    void *second_small_obj_mem = allocator.Alloc(SMALL_OBJ_SIZE);
    size_t second_small_obj_index = SetBytesFromByteArray(second_small_obj_mem, SMALL_OBJ_SIZE);

    ASSERT_TRUE(CompareBytesWithByteArray(big_obj_mem, BIG_OBJ_SIZE, big_obj_index));
    ASSERT_TRUE(CompareBytesWithByteArray(small_obj_mem, SMALL_OBJ_SIZE, small_obj_index));
    ASSERT_TRUE(CompareBytesWithByteArray(second_small_obj_mem, SMALL_OBJ_SIZE, second_small_obj_index));
    delete mem_stats;
}

TEST_F(RunSlotsAllocatorTest, MTAllocFreeTest)
{
    static constexpr size_t MIN_ELEMENTS_COUNT = 1500;
    static constexpr size_t MAX_ELEMENTS_COUNT = 3000;
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 10;
#endif
    static constexpr size_t MT_TEST_RUN_COUNT = 5;
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        MtAllocFreeTest<1, RunSlotsType::MaxSlotSize(), THREADS_COUNT>(MIN_ELEMENTS_COUNT, MAX_ELEMENTS_COUNT);
    }
}

TEST_F(RunSlotsAllocatorTest, MTAllocIterateTest)
{
    static constexpr size_t MIN_ELEMENTS_COUNT = 1500;
    static constexpr size_t MAX_ELEMENTS_COUNT = 3000;
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 10;
#endif
    static constexpr size_t MT_TEST_RUN_COUNT = 5;
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        MtAllocIterateTest<1, RunSlotsType::MaxSlotSize(), THREADS_COUNT>(
            MIN_ELEMENTS_COUNT, MAX_ELEMENTS_COUNT, CrossingMapSingleton::GetCrossingMapGranularity());
    }
}

TEST_F(RunSlotsAllocatorTest, MTAllocCollectTest)
{
    static constexpr size_t MIN_ELEMENTS_COUNT = 1500;
    static constexpr size_t MAX_ELEMENTS_COUNT = 3000;
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 10;
#endif
    static constexpr size_t MT_TEST_RUN_COUNT = 5;
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        MtAllocCollectTest<1, RunSlotsType::MaxSlotSize(), THREADS_COUNT>(MIN_ELEMENTS_COUNT, MAX_ELEMENTS_COUNT);
    }
}

}  // namespace panda::mem
