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

#include "runtime/mem/gc/generational-gc-base.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/panda_vm.h"

namespace panda::mem {

template <class LanguageConfig>
bool GenerationalGC<LanguageConfig>::ShouldRunTenuredGC(const GCTask &task)
{
    return this->IsOnPygoteFork() || task.reason == GCTaskCause::OOM_CAUSE ||
           task.reason == GCTaskCause::EXPLICIT_CAUSE || task.reason == GCTaskCause::HEAP_USAGE_THRESHOLD_CAUSE ||
           task.reason == GCTaskCause::STARTUP_COMPLETE_CAUSE;
}

template <class LanguageConfig>
PandaString GenerationalGC<LanguageConfig>::MemStats::Dump()
{
    PandaStringStream statistic;
    statistic << "Young freed " << young_free_object_count_ << " objects ("
              << helpers::MemoryConverter(young_free_object_size_) << ") Young moved " << young_move_object_count_
              << " objects, " << young_move_object_size_ << " bytes ("
              << helpers::MemoryConverter(young_move_object_size_) << ")";
    if (tenured_free_object_size_ > 0U) {
        statistic << " Tenured freed " << tenured_free_object_size_ << "("
                  << helpers::MemoryConverter(tenured_free_object_size_) << ")";
    }
    return statistic.str();
}

template <class LanguageConfig>
void GenerationalGC<LanguageConfig>::UpdateMemStats(size_t bytes_in_heap_before, bool update_tenured_stats,
                                                    bool record_allocation_for_moved_objects)
{
    size_t young_move_size = this->mem_stats_.GetSizeMovedYoung();
    size_t young_move_count = this->mem_stats_.GetCountMovedYoung();
    size_t young_delete_size = this->mem_stats_.GetSizeFreedYoung();
    size_t young_delete_count = this->mem_stats_.GetCountFreedYoung();
    size_t tenured_move_size = update_tenured_stats ? this->mem_stats_.GetSizeMovedTenured() : 0;
    size_t tenured_move_count = update_tenured_stats ? this->mem_stats_.GetCountMovedTenured() : 0;
    size_t tenured_delete_size = update_tenured_stats ? this->mem_stats_.GetSizeFreedTenured() : 0;
    size_t tenured_delete_count = update_tenured_stats ? this->mem_stats_.GetCountFreedTenured() : 0;

    auto *vm_mem_stats = this->GetPandaVm()->GetMemStats();
    GCInstanceStats *gc_stats = this->GetStats();

    if (record_allocation_for_moved_objects) {
        vm_mem_stats->RecordAllocateObjects(young_move_count + tenured_move_count, young_move_size + tenured_move_size,
                                            SpaceType::SPACE_TYPE_OBJECT);
    }
    if (young_move_size > 0) {
        gc_stats->AddMemoryValue(young_move_size, MemoryTypeStats::MOVED_BYTES);
        gc_stats->AddObjectsValue(young_move_count, ObjectTypeStats::MOVED_OBJECTS);
        vm_mem_stats->RecordYoungMovedObjects(young_move_count, young_move_size, SpaceType::SPACE_TYPE_OBJECT);
    }
    if (young_delete_size > 0) {
        gc_stats->AddMemoryValue(young_delete_size, MemoryTypeStats::YOUNG_FREED_BYTES);
        gc_stats->AddObjectsValue(young_delete_count, ObjectTypeStats::YOUNG_FREED_OBJECTS);
    }

    if (bytes_in_heap_before > 0) {
        gc_stats->AddCopiedRatioValue(static_cast<double>(young_move_size + tenured_move_size) / bytes_in_heap_before);
    }

    if (tenured_move_size > 0) {
        gc_stats->AddMemoryValue(tenured_move_size, MemoryTypeStats::MOVED_BYTES);
        gc_stats->AddObjectsValue(tenured_move_count, ObjectTypeStats::MOVED_OBJECTS);
        vm_mem_stats->RecordTenuredMovedObjects(tenured_move_count, tenured_move_size, SpaceType::SPACE_TYPE_OBJECT);
    }
    if (tenured_delete_size > 0) {
        gc_stats->AddMemoryValue(tenured_delete_size, MemoryTypeStats::ALL_FREED_BYTES);
        gc_stats->AddObjectsValue(tenured_delete_count, ObjectTypeStats::ALL_FREED_OBJECTS);
    }
    vm_mem_stats->RecordFreeObjects(young_delete_count + tenured_delete_count, young_delete_size + tenured_delete_size,
                                    SpaceType::SPACE_TYPE_OBJECT);
}

template <class LanguageConfig>
void GenerationalGC<LanguageConfig>::CreateCardTable(InternalAllocatorPtr internal_allocator_ptr, uintptr_t min_address,
                                                     size_t size)
{
    card_table_ = MakePandaUnique<CardTable>(internal_allocator_ptr, min_address, size);
    card_table_->Initialize();
}

template <class LanguageConfig>
bool GenerationalGC<LanguageConfig>::Trigger(PandaUniquePtr<GCTask> task)
{
    // Check current heap size.
    // Collect Young gen.
    // If threshold for tenured gen - collect tenured gen.
    // NOTE(dtrubenkov): change for concurrent mode
    return this->AddGCTask(true, std::move(task));
}

TEMPLATE_CLASS_LANGUAGE_CONFIG(GenerationalGC);
}  // namespace panda::mem
