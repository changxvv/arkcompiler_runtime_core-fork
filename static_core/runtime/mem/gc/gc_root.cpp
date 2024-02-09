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

#include "runtime/mem/gc/gc_root.h"

#include "libpandafile/panda_cache.h"
#include "runtime/include/object_header.h"
#include "runtime/include/runtime.h"
#include "runtime/include/stack_walker-inl.h"
#include "runtime/mem/refstorage/reference_storage.h"
#include "runtime/mem/gc/card_table-inl.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/gc/gc_root_type.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/refstorage/global_object_storage.h"
#include "runtime/include/panda_vm.h"

namespace ark::mem {

GCRoot::GCRoot(RootType type, ObjectHeader *obj)
{
    type_ = type;
    fromObject_ = nullptr;
    object_ = obj;
}

GCRoot::GCRoot(RootType type, ObjectHeader *fromObject, ObjectHeader *obj)
{
    ASSERT((fromObject != nullptr && type == RootType::ROOT_TENURED) || type != RootType::ROOT_TENURED);
    type_ = type;
    fromObject_ = fromObject;
    object_ = obj;
}

RootType GCRoot::GetType() const
{
    return type_;
}

ObjectHeader *GCRoot::GetObjectHeader() const
{
    return object_;
}

ObjectHeader *GCRoot::GetFromObjectHeader() const
{
    ASSERT((fromObject_ != nullptr && type_ == RootType::ROOT_TENURED) || type_ != RootType::ROOT_TENURED);
    return fromObject_;
}

std::ostream &operator<<(std::ostream &os, const GCRoot &root)
{
    switch (root.GetType()) {
        case RootType::ROOT_CLASS:
            os << "ROOT CLASS";
            break;
        case RootType::ROOT_FRAME:
            os << "ROOT FRAME";
            break;
        case RootType::ROOT_THREAD:
            os << "ROOT THREAD";
            break;
        case RootType::ROOT_TENURED:
            os << "ROOT TENURED";
            break;
        case RootType::ROOT_NATIVE_GLOBAL:
            os << "ROOT NATIVE_GLOBAL";
            break;
        case RootType::ROOT_NATIVE_LOCAL:
            os << "ROOT NATIVE_LOCAL";
            break;
        case RootType::ROOT_AOT_STRING_SLOT:
            os << "ROOT AOT_STRING_SLOT";
            break;
        case RootType::ROOT_UNKNOWN:
        default:
            LOG(FATAL, GC) << "ROOT UNKNOWN";
            break;
    }
    os << std::hex << " " << root.GetObjectHeader() << std::endl;
    return os;
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::VisitNonHeapRoots(const GCRootVisitor &gcRootVisitor, VisitGCRootFlags flags) const
{
    VisitLocalRoots(gcRootVisitor);
    VisitClassRoots(gcRootVisitor, flags);
    VisitAotStringRoots(gcRootVisitor, flags);
    VisitClassLinkerContextRoots(gcRootVisitor);
    VisitVmRoots(gcRootVisitor);
    auto *storage = vm_->GetGlobalObjectStorage();
    if (storage != nullptr) {
        storage->VisitObjects(gcRootVisitor, mem::RootType::ROOT_NATIVE_GLOBAL);
    }
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::VisitCardTableRoots(CardTable *cardTable, ObjectAllocatorBase *allocator,
                                                      GCRootVisitor rootVisitor, MemRangeChecker rangeChecker,
                                                      ObjectChecker rangeObjectChecker, ObjectChecker fromObjectChecker,
                                                      uint32_t processedFlag) const
{
    cardTable->VisitMarked(
        [&allocator, &rootVisitor, &rangeChecker, &rangeObjectChecker, &fromObjectChecker,
         &cardTable](MemRange memRange) {
            if (rangeChecker(memRange)) {
                auto objectsInRangeVisitor = [&rootVisitor, &rangeObjectChecker,
                                              &fromObjectChecker](ObjectHeader *objectHeader) {
                    auto traverseObjectInRange = [&rootVisitor, &rangeObjectChecker](ObjectHeader *fromObject,
                                                                                     ObjectHeader *objectToTraverse) {
                        if (rangeObjectChecker(objectToTraverse)) {
                            // The weak references from dynobjects should not be regarded as roots.
                            TaggedValue value(objectToTraverse);
                            if (!value.IsWeak()) {
                                rootVisitor(GCRoot(RootType::ROOT_TENURED, fromObject, objectToTraverse));
                            }
                        }
                    };
                    if (objectHeader->ClassAddr<BaseClass>() != nullptr && fromObjectChecker(objectHeader)) {
                        // The class may be null in the situation when a new object is allocated in the card
                        // we are visiting now, but the class is not set yet.
                        ObjectHelpers<LanguageConfig::LANG_TYPE>::TraverseAllObjects(objectHeader,
                                                                                     traverseObjectInRange);
                    }
                };
                allocator->IterateOverObjectsInRange(memRange, objectsInRangeVisitor);
            } else {
                cardTable->MarkCard(memRange.GetStartAddress());
            }
        },
        processedFlag);
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::VisitRootsForThread(ManagedThread *thread, const GCRootVisitor &gcRootVisitor) const
{
    LOG(DEBUG, GC) << "Start collecting roots for thread " << thread->GetId();

    thread->VisitGCRoots([&gcRootVisitor](ObjectHeader *obj) {
        LOG(DEBUG, GC) << " Found root for thread" << GetDebugInfoAboutObject(obj);
        gcRootVisitor({RootType::ROOT_THREAD, obj});
    });
    LOG(DEBUG, GC) << "Finish collecting roots for thread " << thread->GetId();
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::VisitLocalRoots(const GCRootVisitor &gcRootVisitor) const
{
    auto threadVisitor = [this, &gcRootVisitor](ManagedThread *thread) {
        VisitRootsForThread(thread, gcRootVisitor);
        for (auto stack = StackWalker::Create(thread); stack.HasFrame(); stack.NextFrame()) {
            LOG(DEBUG, GC) << " VisitRoots frame " << std::hex << stack.GetFp();
            stack.IterateObjects([this, &gcRootVisitor](auto &vreg) {
                this->VisitRegisterRoot(vreg, gcRootVisitor);
                return true;
            });
        }
        return true;
    };
    vm_->GetThreadManager()->EnumerateThreads(threadVisitor);
}

template <class LanguageConfig>
template <class VRegRef>
void RootManager<LanguageConfig>::VisitRegisterRoot(const VRegRef &vRegister, const GCRootVisitor &gcRootVisitor) const
{
    if (UNLIKELY(vRegister.HasObject())) {
        ObjectHeader *objectHeader = vRegister.GetReference();
        if (objectHeader != nullptr) {
            LOG(DEBUG, GC) << " Found root for register" << GetDebugInfoAboutObject(objectHeader);
            gcRootVisitor({RootType::ROOT_FRAME, objectHeader});
        }
    }
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::VisitVmRoots(const GCRootVisitor &gcRootVisitor) const
{
    vm_->VisitVmRoots(gcRootVisitor);
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::VisitAotStringRoots(const GCRootVisitor &gcRootVisitor, VisitGCRootFlags flags) const
{
    trace::ScopedTrace scopedTrace(__FUNCTION__);
    LOG(DEBUG, GC) << "Start collecting AOT string slot roots";
    Runtime::GetCurrent()->GetClassLinker()->GetAotManager()->VisitAotStringRoots(
        [&gcRootVisitor](ObjectHeader **slot) {
            gcRootVisitor({RootType::ROOT_AOT_STRING_SLOT, *slot});
        },
        (flags & VisitGCRootFlags::ACCESS_ROOT_AOT_STRINGS_ONLY_YOUNG) != 0);
    LOG(DEBUG, GC) << "Finish collecting AOT string slot roots";
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::UpdateAotStringRoots()
{
    trace::ScopedTrace scopedTrace(__FUNCTION__);
    LOG(DEBUG, GC) << "=== AOT string slot roots update. BEGIN ===";
    auto hm = vm_->GetHeapManager();
    Runtime::GetCurrent()->GetClassLinker()->GetAotManager()->UpdateAotStringRoots(
        [](ObjectHeader **root) {
            auto rootValue = *root;
            if (rootValue->IsForwarded()) {
                *root = ::ark::mem::GetForwardAddress(rootValue);
            }
        },
        [&hm](const ObjectHeader *root) { return hm->IsObjectInYoungSpace(root); });
    LOG(DEBUG, GC) << "=== AOT string slot roots update. END ===";
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::UpdateVmRefs()
{
    vm_->UpdateVmRefs();
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::UpdateGlobalObjectStorage()
{
    auto globalStorage = vm_->GetGlobalObjectStorage();
    if (globalStorage != nullptr) {
        globalStorage->UpdateMovedRefs();
    }
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::VisitClassRoots(const GCRootVisitor &gcRootVisitor, VisitGCRootFlags flags) const
{
    LOG(DEBUG, GC) << "Start collecting roots for classes";
    auto classLinker = Runtime::GetCurrent()->GetClassLinker();
    auto classRootVisitor = [&gcRootVisitor](Class *cls) {
        gcRootVisitor({RootType::ROOT_CLASS, cls->GetManagedObject()});
        LOG(DEBUG, GC) << " Found class root " << GetDebugInfoAboutObject(cls->GetManagedObject());
        return true;
    };
    auto *extension = classLinker->GetExtension(LanguageConfig::LANG);
    extension->EnumerateClasses(classRootVisitor, flags);

    LOG(DEBUG, GC) << "Finish collecting roots for classes";
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::UpdateThreadLocals()
{
    LOG(DEBUG, GC) << "=== ThreadLocals Update moved. BEGIN ===";
    vm_->GetThreadManager()->EnumerateThreads([](ManagedThread *thread) {
        thread->UpdateGCRoots();
        return true;
    });
    LOG(DEBUG, GC) << "=== ThreadLocals Update moved. END ===";
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::VisitClassLinkerContextRoots(const GCRootVisitor &gcRootVisitor) const
{
    LOG(DEBUG, GC) << "Start collecting roots for class linker contexts";
    auto classLinker = Runtime::GetCurrent()->GetClassLinker();
    auto *extension = classLinker->GetExtension(LanguageConfig::LANG);
    extension->EnumerateContexts([&gcRootVisitor](ClassLinkerContext *ctx) {
        ctx->VisitGCRoots([&gcRootVisitor](ObjectHeader *obj) {
            LOG(DEBUG, GC) << " Found root for class linker context " << GetDebugInfoAboutObject(obj);
            gcRootVisitor({RootType::ROOT_CLASS_LINKER, obj});
        });
        return true;
    });
    LOG(DEBUG, GC) << "Finish collecting roots for class linker contexts";
}

template <class LanguageConfig>
void RootManager<LanguageConfig>::UpdateClassLinkerContextRoots()
{
    auto classLinker = Runtime::GetCurrent()->GetClassLinker();
    auto *extension = classLinker->GetExtension(LanguageConfig::LANG);
    extension->EnumerateContexts([](ClassLinkerContext *ctx) {
        ctx->UpdateGCRoots();
        return true;
    });
}

uint32_t operator&(VisitGCRootFlags left, VisitGCRootFlags right)
{
    return static_cast<uint32_t>(left) & static_cast<uint32_t>(right);
}

VisitGCRootFlags operator|(VisitGCRootFlags left, VisitGCRootFlags right)
{
    return static_cast<VisitGCRootFlags>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

TEMPLATE_CLASS_LANGUAGE_CONFIG(RootManager);

}  // namespace ark::mem
