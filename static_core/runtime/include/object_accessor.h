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
#ifndef PANDA_RUNTIME_OBJECT_ACCESSOR_H_
#define PANDA_RUNTIME_OBJECT_ACCESSOR_H_

#include <cstddef>

#include "runtime/mem/gc/gc_barrier_set.h"

namespace panda {

class ObjectHeader;
class Field;
class ManagedThread;

class ObjectAccessor {
public:
    template <class T, bool IS_VOLATILE = false>
    static T GetPrimitive(const void *obj, size_t offset)
    {
        return Get<T, IS_VOLATILE>(obj, offset);
    }

    template <class T, bool IS_VOLATILE = false>
    static void SetPrimitive(void *obj, size_t offset, T value)
    {
        Set<T, IS_VOLATILE>(obj, offset, value);
    }

    template <bool IS_VOLATILE = false, bool NEED_READ_BARRIER = true, bool IS_DYN = false>
    static ObjectHeader *GetObject(const void *obj, size_t offset);

    template <bool IS_VOLATILE = false, bool NEED_WRITE_BARRIER = true, bool IS_DYN = false>
    static void SetObject(void *obj, size_t offset, ObjectHeader *value);

    template <class T>
    static T GetFieldPrimitive(const void *obj, const Field &field);

    template <class T>
    static void SetFieldPrimitive(void *obj, const Field &field, T value);

    template <bool NEED_READ_BARRIER = true, bool IS_DYN = false>
    static ObjectHeader *GetFieldObject(const void *obj, const Field &field);

    template <bool NEED_WRITE_BARRIER = true, bool IS_DYN = false>
    static void SetFieldObject(void *obj, const Field &field, ObjectHeader *value);

    // Pass thread parameter to speed up interpreter
    template <bool IS_VOLATILE = false, bool NEED_READ_BARRIER = true, bool IS_DYN = false>
    static ObjectHeader *GetObject(const ManagedThread *thread, const void *obj, size_t offset);

    template <bool IS_VOLATILE = false, bool NEED_WRITE_BARRIER = true, bool IS_DYN = false>
    static void SetObject(const ManagedThread *thread, void *obj, size_t offset, ObjectHeader *value);

    template <bool NEED_READ_BARRIER = true, bool IS_DYN = false>
    static ObjectHeader *GetFieldObject(const ManagedThread *thread, const void *obj, const Field &field);

    template <bool NEED_WRITE_BARRIER = true, bool IS_DYN = false>
    static void SetFieldObject(const ManagedThread *thread, void *obj, const Field &field, ObjectHeader *value);

    template <class T>
    static T GetFieldPrimitive(const void *obj, size_t offset, std::memory_order memory_order);

    template <class T>
    static void SetFieldPrimitive(void *obj, size_t offset, T value, std::memory_order memory_order);

    template <bool NEED_READ_BARRIER = true, bool IS_DYN = false>
    static ObjectHeader *GetFieldObject(const void *obj, int offset, std::memory_order memory_order);

    template <bool NEED_WRITE_BARRIER = true, bool IS_DYN = false>
    static void SetFieldObject(void *obj, size_t offset, ObjectHeader *value, std::memory_order memory_order);

    template <typename T>
    static std::pair<bool, T> CompareAndSetFieldPrimitive(void *obj, size_t offset, T old_value, T new_value,
                                                          std::memory_order memory_order, bool strong);

    template <bool NEED_WRITE_BARRIER = true, bool IS_DYN = false>
    static std::pair<bool, ObjectHeader *> CompareAndSetFieldObject(void *obj, size_t offset, ObjectHeader *old_value,
                                                                    ObjectHeader *new_value,
                                                                    std::memory_order memory_order, bool strong);

    template <typename T>
    static T GetAndSetFieldPrimitive(void *obj, size_t offset, T value, std::memory_order memory_order);

    template <bool NEED_WRITE_BARRIER = true, bool IS_DYN = false>
    static ObjectHeader *GetAndSetFieldObject(void *obj, size_t offset, ObjectHeader *value,
                                              std::memory_order memory_order);

    template <typename T>
    static T GetAndAddFieldPrimitive(void *obj, size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    static T GetAndBitwiseOrFieldPrimitive(void *obj, size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    static T GetAndBitwiseAndFieldPrimitive(void *obj, size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    static T GetAndBitwiseXorFieldPrimitive(void *obj, size_t offset, T value, std::memory_order memory_order);

    static inline void SetDynValueWithoutBarrier(void *obj, size_t offset, coretypes::TaggedType value);

    static inline void SetDynValue(const ManagedThread *thread, void *obj, size_t offset, coretypes::TaggedType value);

    template <typename T>
    static inline void SetDynPrimitive(const ManagedThread *thread, void *obj, size_t offset, T value);

    template <class T>
    static inline T GetDynValue(const void *obj, size_t offset)
    {
        uintptr_t addr = ToUintPtr(obj) + offset;
        ASSERT(IsAddressInObjectsHeap(addr));
        // Atomic with relaxed order reason: concurrent access from GC
        return reinterpret_cast<const std::atomic<T> *>(addr)->load(std::memory_order_relaxed);
    }

    static void SetClass(ObjectHeader *obj, BaseClass *new_class);

private:
    template <class T, bool IS_VOLATILE>
    static T Get(const void *obj, size_t offset)
    {
        auto *addr = reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(obj) + offset);
        ASSERT(IsAddressInObjectsHeap(addr));
        if (IS_VOLATILE) {
            // Atomic with seq_cst order reason: required for volatile
            return reinterpret_cast<const std::atomic<T> *>(addr)->load(std::memory_order_seq_cst);
        }
        // Atomic with relaxed order reason: to be compatible with other vms
        return reinterpret_cast<const std::atomic<T> *>(addr)->load(std::memory_order_relaxed);
    }

    template <class T, bool IS_VOLATILE>
    static void Set(void *obj, size_t offset, T value)
    {
        auto *addr = reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(obj) + offset);
        ASSERT(IsAddressInObjectsHeap(addr));
        if (IS_VOLATILE) {
            // Atomic with seq_cst order reason: required for volatile
            return reinterpret_cast<std::atomic<T> *>(addr)->store(value, std::memory_order_seq_cst);
        }
        // Atomic with relaxed order reason: to be compatible with other vms
        return reinterpret_cast<std::atomic<T> *>(addr)->store(value, std::memory_order_relaxed);
    }

    template <class T>
    static T Get(const void *obj, size_t offset, std::memory_order memory_order)
    {
        auto *addr = reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(obj) + offset);
        ASSERT(IsAddressInObjectsHeap(addr));
        // Atomic with parameterized order reason: memory order passed as argument
        return reinterpret_cast<const std::atomic<T> *>(addr)->load(memory_order);
    }

    template <class T>
    static void Set(void *obj, size_t offset, T value, std::memory_order memory_order)
    {
        auto *addr = reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(obj) + offset);
        ASSERT(IsAddressInObjectsHeap(addr));
        // Atomic with parameterized order reason: memory order passed as argument
        return reinterpret_cast<std::atomic<T> *>(addr)->store(value, memory_order);
    }

    PANDA_PUBLIC_API static mem::GCBarrierSet *GetBarrierSet();

    PANDA_PUBLIC_API static mem::GCBarrierSet *GetBarrierSet(const ManagedThread *thread);

    static mem::BarrierType GetPreBarrierType(const ManagedThread *thread);

    PANDA_PUBLIC_API static mem::BarrierType GetPostBarrierType(const ManagedThread *thread);
};

}  // namespace panda

#endif  // PANDA_RUNTIME_OBJECT_ACCESSOR_H_
