/**
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

#include <cstddef>
#include <cstdint>
#include <optional>
#include "libpandabase/macros.h"
#include "libpandabase/utils/bit_utils.h"
#include "mem/vm_handle.h"
#include "runtime/include/runtime.h"
#include "runtime/include/object_header.h"
#include "runtime/include/thread_scopes.h"
#include "runtime/include/mem/panda_containers.h"
#include "plugins/ets/runtime/ets_handle.h"
#include "plugins/ets/runtime/ets_coroutine.h"
#include "plugins/ets/runtime/types/ets_class.h"
#include "plugins/ets/runtime/types/ets_shared_memory.h"

namespace panda::ets {

bool EtsSharedMemory::Waiter::Wait(std::optional<uint64_t> timeout)
{
    auto mutex = &EtsCoroutine::GetCurrent()->GetPandaVM()->GetAtomicsMutex();
    if (timeout.has_value()) {
        auto ms = timeout.value();
        return cv_.TimedWait(mutex, ms);
    }

    cv_.Wait(mutex);
    return false;
}

void EtsSharedMemory::Waiter::SignalAll()
{
    cv_.SignalAll();
}

EtsSharedMemory *EtsSharedMemory::Create(size_t length)
{
    auto *current_coro = EtsCoroutine::GetCurrent();
    [[maybe_unused]] EtsHandleScope scope(current_coro);

    auto cls = current_coro->GetPandaVM()->GetClassLinker()->GetSharedMemoryClass();
    // Note: This object must be non-movable since the 'waiter_' pointer is shared between different threads
    EtsHandle<EtsSharedMemory> hmem(current_coro, EtsSharedMemory::FromEtsObject(EtsObject::CreateNonMovable(cls)));
    auto *array_ptr = EtsByteArray::Create(length, SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
    ObjectAccessor::SetObject(current_coro, hmem.GetPtr(), MEMBER_OFFSET(EtsSharedMemory, array_),
                              array_ptr->GetCoreType());
    hmem->SetHeadWaiter(nullptr);
    return hmem.GetPtr();
}

void EtsSharedMemory::LinkWaiter(Waiter &waiter)
{
    waiter.SetPrev(nullptr);
    waiter.SetNext(GetHeadWaiter());
    SetHeadWaiter(&waiter);
}

void EtsSharedMemory::UnlinkWaiter(Waiter &waiter)
{
    auto prev = waiter.GetPrev();
    auto next = waiter.GetNext();

    if (prev != nullptr) {
        prev->SetNext(next);
    }
    if (next != nullptr) {
        next->SetPrev(prev);
    }
    if (GetHeadWaiter() == &waiter) {
        SetHeadWaiter(nullptr);
    }
}

size_t EtsSharedMemory::GetLength()
{
    auto *current_coro = EtsCoroutine::GetCurrent();
    auto *array_ptr = reinterpret_cast<EtsByteArray *>(
        ObjectAccessor::GetObject(current_coro, this, MEMBER_OFFSET(EtsSharedMemory, array_)));
    return array_ptr->GetLength();
}

int8_t EtsSharedMemory::GetElement(uint32_t index)
{
    auto *current_coro = EtsCoroutine::GetCurrent();
    auto *array_ptr = reinterpret_cast<EtsByteArray *>(
        ObjectAccessor::GetObject(current_coro, this, MEMBER_OFFSET(EtsSharedMemory, array_)));
    ASSERT_PRINT(index < GetLength(), "SharedMemory index out of bounds");
    return array_ptr->Get(index);
}

void EtsSharedMemory::SetElement(uint32_t index, int8_t element)
{
    auto *current_coro = EtsCoroutine::GetCurrent();
    auto *array_ptr = reinterpret_cast<EtsByteArray *>(
        ObjectAccessor::GetObject(current_coro, this, MEMBER_OFFSET(EtsSharedMemory, array_)));
    ASSERT_PRINT(index < GetLength(), "SharedMemory index out of bounds");
    array_ptr->Set(index, element);
}

namespace {

std::string PrintWaiters(EtsHandle<EtsSharedMemory> &buffer)
{
    std::stringstream stream;
    auto cur_waiter = buffer->GetHeadWaiter();
    while (cur_waiter != nullptr) {
        stream << reinterpret_cast<size_t>(cur_waiter) << " -> ";
        cur_waiter = cur_waiter->GetNext();
    }
    return stream.str();
}

bool IsLittleEndian()
{
    int32_t x = 1;
    return *reinterpret_cast<int8_t *>(&x) == static_cast<int8_t>(1);
}

template <typename IntegerType, typename UIntegerType>
IntegerType AssembleFromBytes(EtsSharedMemory &mem, uint32_t index, uint32_t (*get_byte_index)(uint32_t, uint32_t))
{
    UIntegerType value = 0;
    for (uint32_t i = 0; i < sizeof(IntegerType); i++) {
        auto cur_byte_index = get_byte_index(index, i);
        auto cur_byte = bit_cast<UIntegerType>(static_cast<IntegerType>(mem.GetElement(cur_byte_index)));
        value |= cur_byte << (8U * i);
    }
    return bit_cast<IntegerType>(value);
}

uint32_t LittleEndianGetByteIndex(uint32_t index, uint32_t i)
{
    return index + i;
}

template <typename IntegerType>
uint32_t BigEndianGetByteIndex(uint32_t index, uint32_t i)
{
    return index + sizeof(IntegerType) - 1 - i;
}

template <typename IntegerType, typename UIntegerType>
IntegerType AssembleFromBytes(EtsSharedMemory &mem, uint32_t index)
{
    return IsLittleEndian()
               ? AssembleFromBytes<IntegerType, UIntegerType>(mem, index, LittleEndianGetByteIndex)
               : AssembleFromBytes<IntegerType, UIntegerType>(mem, index, BigEndianGetByteIndex<IntegerType>);
}

template <typename IntegerType, typename UIntegerType>
EtsSharedMemory::WaitResult Wait(EtsSharedMemory *mem, uint32_t byte_offset, IntegerType expected_value,
                                 std::optional<uint64_t> timeout)
{
    auto coroutine = EtsCoroutine::GetCurrent();
    [[maybe_unused]] EtsHandleScope scope(coroutine);
    EtsHandle<EtsSharedMemory> this_handle(coroutine, mem);

    ScopedNativeCodeThread n(coroutine);
    os::memory::LockHolder lock(coroutine->GetPandaVM()->GetAtomicsMutex());
    ScopedManagedCodeThread m(coroutine);

    auto witnessed_value = AssembleFromBytes<IntegerType, UIntegerType>(*(this_handle.GetPtr()), byte_offset);
    LOG(DEBUG, ATOMICS) << "Wait: witnesseed_value=" << witnessed_value << ", expected_value=" << expected_value;
    if (witnessed_value == expected_value) {
        // Only stack allocations

        // 1. Add waiter
        auto waiter = EtsSharedMemory::Waiter(byte_offset);
        this_handle->LinkWaiter(waiter);
        LOG(DEBUG, ATOMICS) << "Wait: added waiter: " << reinterpret_cast<size_t>(&waiter)
                            << ", list: " << PrintWaiters(this_handle);

        // 2. Wait
        bool timed_out = false;
        while (!waiter.IsNotified() && !timed_out) {
            ScopedNativeCodeThread n_cv(coroutine);

            timed_out = waiter.Wait(timeout);
            LOG(DEBUG, ATOMICS) << "Wait: woke up, waiter: " << reinterpret_cast<size_t>(&waiter);
        }

        // 3. Remove waiter
        this_handle->UnlinkWaiter(waiter);
        LOG(DEBUG, ATOMICS) << "Wait: removed waiter: " << reinterpret_cast<size_t>(&waiter)
                            << ", list: " << PrintWaiters(this_handle);
        return timed_out ? EtsSharedMemory::WaitResult::TIMED_OUT : EtsSharedMemory::WaitResult::OK;
    }

    LOG(DEBUG, ATOMICS) << "Wait: not-equal, returning";
    return EtsSharedMemory::WaitResult::NOT_EQUAL;
}

}  // namespace

EtsSharedMemory::WaitResult EtsSharedMemory::WaitI32(uint32_t byte_offset, int32_t expected_value,
                                                     std::optional<uint64_t> timeout)
{
    return Wait<int32_t, uint32_t>(this, byte_offset, expected_value, timeout);
}

EtsSharedMemory::WaitResult EtsSharedMemory::WaitI64(uint32_t byte_offset, int64_t expected_value,
                                                     std::optional<uint64_t> timeout)
{
    return Wait<int64_t, uint64_t>(this, byte_offset, expected_value, timeout);
}

int32_t EtsSharedMemory::NotifyI32(uint32_t byte_offset, std::optional<uint32_t> count)
{
    auto coroutine = EtsCoroutine::GetCurrent();
    [[maybe_unused]] EtsHandleScope scope(coroutine);
    EtsHandle<EtsSharedMemory> this_handle(coroutine, this);

    ScopedNativeCodeThread n(coroutine);
    os::memory::LockHolder lock(coroutine->GetPandaVM()->GetAtomicsMutex());
    ScopedManagedCodeThread m(coroutine);

    auto waiter = this_handle->GetHeadWaiter();
    uint32_t notified_count = 0;
    LOG(DEBUG, ATOMICS) << "Notify: started, head waiter: " << reinterpret_cast<size_t>(waiter);
    while (waiter != nullptr && (!count.has_value() || notified_count < count.value())) {
        LOG(DEBUG, ATOMICS) << "Notify: vising waiter: " << reinterpret_cast<size_t>(waiter)
                            << ", list: " << PrintWaiters(this_handle);
        if (waiter->GetOffset() == byte_offset) {
            ScopedNativeCodeThread n_cv(coroutine);

            waiter->SetNotified();
            LOG(DEBUG, ATOMICS) << "Notify: notifying waiter " << reinterpret_cast<size_t>(waiter)
                                << ", list: " << PrintWaiters(this_handle);
            waiter->SignalAll();
            notified_count += 1;
        }
        waiter = waiter->GetNext();
    }
    LOG(DEBUG, ATOMICS) << "Notify: notified " << notified_count << " waiters";

    return static_cast<int32_t>(notified_count);
}

}  // namespace panda::ets